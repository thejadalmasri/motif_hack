# Bridge server: JUCE plugin → Stable Audio 3 API
# pip3 install flask requests
#
# Architecture:
#   JUCE plugin → POST http://127.0.0.1:8080/process  (this server)
#                      ↓
#                forwards to → http://127.0.0.1:8765/(inpaint|continue)
#                      ↓
#                returns WAV back to JUCE
#
# Run order:
#   1. Start stable_audio_server.py first (port 8765)
#   2. Start this server.py (port 8080)
#   3. JUCE plugin calls this server on port 8080
#
# JUCE → Bridge contract:
#   audio       : WAV file (required)
#   mode        : "append" or "restyle"
#   prompt      : text
#   start       : SECONDS (float)  — used in append mode for mask_start
#   end         : SECONDS (float)  — used in append mode for mask_end
#   creativity  : 0–100  — used in restyle mode, normalized to 0.0–1.0

from flask import Flask, request, send_file, jsonify
import requests
import io
import wave
import contextlib
import tempfile
import os

app = Flask(__name__)

STABLE_AUDIO_URL = 'http://127.0.0.1:8765'

DEFAULT_PROMPT = 'Arabic string ensemble playing a traditional melody'
DEFAULT_DURATION = 30.0
DEFAULT_CREATIVITY = 55
DEFAULT_STEPS = 8
DEFAULT_CFG_SCALE = 1.0


def _get_wav_duration_seconds(wav_bytes: bytes) -> float:
    try:
        with contextlib.closing(wave.open(io.BytesIO(wav_bytes), 'rb')) as w:
            frames = w.getnframes()
            rate = w.getframerate()
            return frames / float(rate) if rate else DEFAULT_DURATION
    except Exception as e:
        print(f'[warn] could not read WAV duration: {e}')
        return DEFAULT_DURATION


@app.route('/process', methods=['POST'])
def process():
    if 'audio' not in request.files:
        return jsonify({'error': 'audio file required'}), 400

    audio_file = request.files['audio']
    audio_bytes = audio_file.read()

    if not audio_bytes.startswith(b'RIFF'):
        print(f'[warn] received non-WAV bytes (first 16): {audio_bytes[:16]!r}')
        return jsonify({'error': 'uploaded file is not a valid WAV (no RIFF header)'}), 400

    input_duration = _get_wav_duration_seconds(audio_bytes)

    # JUCE form fields (mode lowercase from JUCE)
    mode = request.form.get('mode', 'restyle').lower()
    prompt = request.form.get('prompt', DEFAULT_PROMPT) or DEFAULT_PROMPT

    # start/end are now SECONDS (float), sent directly by JUCE
    mask_start = float(request.form.get('start', 3.0))
    mask_end = float(request.form.get('end', 11.0))

    # creativity is still 0–100, normalize to 0–1
    creativity_pct = float(request.form.get('creativity', DEFAULT_CREATIVITY))
    noise_level = max(0.0, min(1.0, creativity_pct / 100.0))

    # Clamp mask to input duration so we don't ask the model to inpaint past EOF
    mask_start = max(0.0, min(mask_start, input_duration))
    mask_end = max(mask_start, min(mask_end, input_duration))

    steps = int(request.form.get('steps', DEFAULT_STEPS))
    cfg_scale = float(request.form.get('cfg_scale', DEFAULT_CFG_SCALE))

    print(f'\n[process] mode={mode}  prompt="{prompt}"')
    print(f'          input_duration={input_duration:.2f}s')
    if mode == 'append':
        print(f'          mask: {mask_start:.2f}s → {mask_end:.2f}s')
    else:
        print(f'          creativity={creativity_pct}/100 → noise={noise_level:.2f}')

    # Write WAV to temp file so requests sends a real file handle
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
            tmp.write(audio_bytes)
            tmp_path = tmp.name

        with open(tmp_path, 'rb') as f:
            if mode == 'append':
                response = requests.post(
                    f'{STABLE_AUDIO_URL}/inpaint',
                    files={'audio_file': ('input.wav', f, 'audio/wav')},
                    data={
                        'prompt': prompt,
                        'mask_start': mask_start,
                        'mask_end': mask_end,
                        'steps': steps,
                        'cfg_scale': cfg_scale,
                    },
                    timeout=600,
                )
            else:
                response = requests.post(
                    f'{STABLE_AUDIO_URL}/continue',
                    files={'audio_file': ('input.wav', f, 'audio/wav')},
                    data={
                        'prompt': prompt,
                        'duration': input_duration,
                        'noise_level': noise_level,
                        'steps': steps,
                        'cfg_scale': cfg_scale,
                    },
                    timeout=600,
                )

        if response.status_code != 200:
            return jsonify({
                'error': 'stable audio API error',
                'status': response.status_code,
                'detail': response.text,
            }), 500

        print(f'[process] success — returned {len(response.content)} bytes\n')
        return send_file(
            io.BytesIO(response.content),
            mimetype='audio/wav',
            as_attachment=False,
            download_name='generated.wav',
        )

    except requests.exceptions.ConnectionError:
        return jsonify({
            'error': 'Stable Audio server not running. '
                     'Start stable_audio_server.py on port 8765 first.'
        }), 503
    except requests.exceptions.Timeout:
        return jsonify({'error': 'generation timed out (10 min limit)'}), 504
    except Exception as e:
        return jsonify({'error': str(e)}), 500
    finally:
        if tmp_path and os.path.exists(tmp_path):
            try:
                os.unlink(tmp_path)
            except Exception:
                pass


@app.route('/health', methods=['GET'])
def health():
    try:
        r = requests.get(f'{STABLE_AUDIO_URL}/health', timeout=5)
        backend = r.json() if r.status_code == 200 else {'status': 'error'}
    except Exception as e:
        backend = {'status': 'unreachable', 'error': str(e)}
    return jsonify({'bridge': 'ok', 'bridge_port': 8080, 'backend': backend})


if __name__ == '__main__':
    print('==========================================')
    print('  JUCE → Stable Audio Bridge Server')
    print('==========================================')
    print(f'  Listening:  http://127.0.0.1:8080')
    print(f'  Forwarding: {STABLE_AUDIO_URL}')
    print('==========================================')
    print('  JUCE contract:')
    print('    mode:       "append" or "restyle"')
    print('    start/end:  SECONDS (float)')
    print('    creativity: 0-100 (mapped to noise 0.0-1.0)')
    print('==========================================\n')
    app.run(host='127.0.0.1', port=8080)
