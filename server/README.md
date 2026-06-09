# MOTIF — Server components

This folder contains two HTTP servers that together expose the Stable Audio 3 + maqam LoRA inference engine to the JUCE plugin.

```
JUCE plugin → Flask bridge (8080) → FastAPI inference (8765) → Stable Audio 3
```

The split exists because the JUCE plugin's HTTP client conventions (multipart form fields like `start`, `end`, `creativity` as 0–100 ints) don't match the more granular contract of the model API (seconds as floats, noise level as 0.0–1.0). The bridge translates between them.

---

## Files

- `stable_audio_server.py` — FastAPI inference server. Loads the model + LoRA at startup, exposes `/generate`, `/inpaint`, `/continue`, `/maqam`, and `/health` endpoints. Listens on **port 8765**.
- `server.py` — Flask bridge. Receives the multipart `/process` request from JUCE and forwards it (translated) to the FastAPI server. Listens on **port 8080**.
- `requirements.txt` — Python dependencies for the bridge.

---

## Setup

The FastAPI server runs inside the Stable Audio 3 venv (with PyTorch, MPS support, etc.). The Flask bridge runs in your system Python.

### FastAPI server dependencies

These come from the Stable Audio 3 repository's `uv sync --extra lora`. The server also needs:

```bash
cd /path/to/stable-audio-3
uv pip install fastapi uvicorn soundfile python-multipart
```

### Flask bridge dependencies

```bash
pip3 install -r requirements.txt
```

---

## Running

### Start the FastAPI inference server (terminal 1)

```bash
cd /path/to/stable-audio-3
uv run python /path/to/motif_hack/server/stable_audio_server.py \
    --model medium \
    --lora-ckpt-path lora_checkpoints/maqam-step=10000-epoch=714.safetensors \
    --lora-strength 1.0
```

First time it runs, the medium model will download from HuggingFace (~5 GB).

When it's ready you'll see:

```
Model ready in 6.9s  |  device=mps
API running at http://127.0.0.1:8765
Docs at       http://127.0.0.1:8765/docs
```

The `/docs` endpoint gives you an interactive Swagger UI for testing every endpoint.

### Start the Flask bridge (terminal 2)

```bash
cd /path/to/motif_hack/server
python3 server.py
```

Output:

```
==========================================
  JUCE → Stable Audio Bridge Server
==========================================
  Listening:  http://127.0.0.1:8080
  Forwarding: http://127.0.0.1:8765
==========================================
```

### Verify

```bash
curl http://127.0.0.1:8080/health
```

You should see both `bridge: ok` and the FastAPI backend health (showing the LoRA path and model name).

---

## API contracts

### FastAPI server (port 8765)

Five endpoints. See the interactive docs at `http://127.0.0.1:8765/docs` while it's running.

| Endpoint | Purpose |
|----------|---------|
| `POST /generate` | Text-to-audio from prompt only |
| `POST /inpaint` | Regenerate a time region of an uploaded audio file |
| `POST /continue` | Use uploaded audio as a noisy starting point, regenerate the whole piece |
| `POST /maqam` | Convenience endpoint that auto-builds a maqam-aware prompt |
| `GET /health` | Verifies model + LoRA loaded |

All audio responses are `audio/wav`, 44.1 kHz.

### Flask bridge (port 8080)

Single endpoint for the JUCE plugin:

```
POST /process
Content-Type: multipart/form-data
```

| Field | Type | Description |
|-------|------|-------------|
| `audio` | file (WAV) | Input audio from the DAW |
| `mode` | string | `"append"` or `"restyle"` |
| `prompt` | string | Text description of desired output |
| `start` | float | Inpaint region start in seconds (used in `append` mode) |
| `end` | float | Inpaint region end in seconds (used in `append` mode) |
| `creativity` | int 0–100 | Mapped to noise level 0.0–1.0 (used in `restyle` mode) |

Returns: `audio/wav` binary.

Internally, the bridge translates:
- `append` → FastAPI `/inpaint` with `mask_start`/`mask_end` in seconds
- `restyle` → FastAPI `/continue` with `noise_level = creativity / 100.0`

`GET /health` returns the status of both servers.

---

## Testing without the plugin

You can test the full pipeline with `curl` before installing the VST:

```bash
# Test inference server directly
curl -X POST http://127.0.0.1:8765/maqam \
    -F "maqam=Hijaz" \
    -F "instrument=qanun" \
    -F "duration=15" \
    -F "steps=50" \
    -o test_direct.wav

# Test through the bridge (simulates what JUCE sends)
curl -X POST http://127.0.0.1:8080/process \
    -F "audio=@/path/to/input.wav" \
    -F "mode=restyle" \
    -F "prompt=Arabic strings melody in Maqam Bayati" \
    -F "creativity=70" \
    -o test_bridge.wav
```

If both succeed and you get audible audio, the backend is working and any further issues are in the plugin or extension.

---

## Performance notes

- Generation on Apple Silicon M5 24GB (MPS device): roughly 30–90 seconds per request at 50 steps, depending on prompt complexity and output duration.
- The medium model holds ~5.5 GB resident in unified memory during inference.
- Reducing `steps` to 8–20 produces noticeably lower quality but useful drafts in 5–10 seconds.
- An H100 GPU would push inference to a few seconds per generation, but this proof of concept ships with the MPS path because Apple Silicon laptops are what the team actually demos on.

---

## Known issues

- The bridge writes the incoming WAV to a temp file before forwarding to FastAPI. This is intentional — passing the bytes directly via `io.BytesIO` triggered a libsndfile error on WAVs with non-standard chunk structures.
- A previous version of the JUCE plugin had a bug where the input WAV temp file was not deleted between runs, causing it to balloon to several hundred MB. That is fixed in the current `PluginProcessor.cpp` (calls `tempFile.deleteFile()` before writing).
- The `lora_strength` parameter is set at server startup and not currently exposed per-request through the bridge. Restarting the FastAPI server with a different `--lora-strength` is the way to change it.
