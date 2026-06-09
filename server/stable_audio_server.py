"""
Stable Audio 3 — Local API Server
Wraps generation, inpainting, and continuation via HTTP.
Run: uv run python stable_audio_server.py --model medium [--lora-ckpt-path ...]
"""

import argparse
import io
import os
import tempfile
import time

import numpy as np
import soundfile as sf
import torch
import uvicorn
from fastapi import FastAPI, File, Form, HTTPException, UploadFile
from fastapi.responses import Response, JSONResponse
from pydantic import BaseModel
from typing import Optional

# ---------------------------------------------------------------------------
# CLI args
# ---------------------------------------------------------------------------

parser = argparse.ArgumentParser()
parser.add_argument("--model", default="small-music",
                    choices=["small-music", "small-music-base", "medium", "medium-base"],
                    help="Model to load")
parser.add_argument("--lora-ckpt-path", default=None,
                    help="Path to .safetensors LoRA checkpoint (optional)")
parser.add_argument("--lora-strength", type=float, default=1.0,
                    help="LoRA strength (0.0–1.0)")
parser.add_argument("--host", default="127.0.0.1")
parser.add_argument("--port", type=int, default=8765)
parser.add_argument("--device", default=None,
                    help="cuda / mps / cpu (auto-detected if not set)")
args, _ = parser.parse_known_args()

# ---------------------------------------------------------------------------
# Model loading
# ---------------------------------------------------------------------------

print(f"Loading model: {args.model} ...")
t0 = time.time()

from stable_audio_3.model import StableAudioModel

model = StableAudioModel.from_pretrained(args.model, device=args.device)

if args.lora_ckpt_path:
    print(f"Loading LoRA: {args.lora_ckpt_path}")
    model.load_lora([args.lora_ckpt_path])
    model.set_lora_strength(args.lora_strength)

print(f"Model ready in {time.time() - t0:.1f}s  |  device={model.device}")

SAMPLE_RATE = model.model.sample_rate

# ---------------------------------------------------------------------------
# FastAPI app
# ---------------------------------------------------------------------------

app = FastAPI(title="Stable Audio 3 API", version="1.0")


def _tensor_to_wav_bytes(audio: torch.Tensor, sr: int) -> bytes:
    """Convert (B, C, T) or (C, T) tensor to WAV bytes."""
    if audio.dim() == 3:
        audio = audio[0]
    audio_np = audio.cpu().float().numpy()
    audio_np = np.clip(audio_np, -1.0, 1.0)
    if audio_np.shape[0] == 1:
        audio_np = audio_np[0]
    else:
        audio_np = audio_np.T
    buf = io.BytesIO()
    sf.write(buf, audio_np, sr, format="WAV", subtype="PCM_16")
    buf.seek(0)
    return buf.read()


def _upload_to_tensor(file_bytes: bytes) -> tuple[int, torch.Tensor]:
    """
    Read uploaded audio bytes → (sample_rate, tensor).
    Writes to a temp file first so libsndfile reads from disk
    (handles JUNK chunks and large files more reliably than BytesIO).
    """
    print(f'[upload] received {len(file_bytes)} bytes, first 16: {file_bytes[:16]!r}')

    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as tmp:
        tmp.write(file_bytes)
        tmp_path = tmp.name

    try:
        data, sr = sf.read(tmp_path, always_2d=True)
        tensor = torch.from_numpy(data.T).float()
        print(f'[upload] decoded: sr={sr}, shape={tensor.shape}')
        return sr, tensor
    finally:
        try:
            os.unlink(tmp_path)
        except Exception:
            pass


# ---------------------------------------------------------------------------
# Endpoints
# ---------------------------------------------------------------------------

@app.get("/health")
def health():
    return {
        "status": "ok",
        "model": args.model,
        "lora": args.lora_ckpt_path,
        "device": str(model.device),
        "sample_rate": SAMPLE_RATE,
    }


@app.post("/generate", summary="Text-to-audio generation")
async def generate(
    prompt: str = Form(..., description="Text prompt"),
    duration: float = Form(10.0, description="Duration in seconds"),
    steps: int = Form(100, description="Diffusion steps"),
    cfg_scale: float = Form(7.0, description="Classifier-free guidance scale"),
    seed: int = Form(-1, description="Random seed (-1 = random)"),
    lora_strength: float = Form(1.0, description="LoRA strength override (0.0–1.0)"),
):
    if args.lora_ckpt_path:
        model.set_lora_strength(lora_strength)

    audio = model.generate(
        prompt=prompt,
        duration=duration,
        steps=steps,
        cfg_scale=cfg_scale,
        seed=seed,
    )
    wav_bytes = _tensor_to_wav_bytes(audio, SAMPLE_RATE)
    return Response(content=wav_bytes, media_type="audio/wav")


@app.post("/inpaint", summary="Inpaint a region of an audio file")
async def inpaint(
    audio_file: UploadFile = File(...),
    prompt: str = Form(...),
    mask_start: float = Form(...),
    mask_end: float = Form(...),
    steps: int = Form(100),
    cfg_scale: float = Form(7.0),
    seed: int = Form(-1),
    lora_strength: float = Form(1.0),
):
    if mask_start >= mask_end:
        raise HTTPException(400, "mask_start must be less than mask_end")

    file_bytes = await audio_file.read()
    sr, audio_tensor = _upload_to_tensor(file_bytes)
    duration = audio_tensor.shape[-1] / sr

    if args.lora_ckpt_path:
        model.set_lora_strength(lora_strength)

    audio_out = model.generate(
        prompt=prompt,
        duration=duration,
        steps=steps,
        cfg_scale=cfg_scale,
        seed=seed,
        inpaint_audio=(sr, audio_tensor),
        inpaint_mask_start_seconds=mask_start,
        inpaint_mask_end_seconds=mask_end,
    )
    wav_bytes = _tensor_to_wav_bytes(audio_out, SAMPLE_RATE)
    return Response(content=wav_bytes, media_type="audio/wav")


@app.post("/continue", summary="Continue / vary audio from an initial clip")
async def continue_audio(
    audio_file: UploadFile = File(...),
    prompt: str = Form(...),
    duration: float = Form(30.0),
    noise_level: float = Form(0.5, description="0=copy, 1=ignore"),
    steps: int = Form(100),
    cfg_scale: float = Form(7.0),
    seed: int = Form(-1),
    lora_strength: float = Form(1.0),
):
    file_bytes = await audio_file.read()
    sr, audio_tensor = _upload_to_tensor(file_bytes)

    if args.lora_ckpt_path:
        model.set_lora_strength(lora_strength)

    audio_out = model.generate(
        prompt=prompt,
        duration=duration,
        steps=steps,
        cfg_scale=cfg_scale,
        seed=seed,
        init_audio=(sr, audio_tensor),
        init_noise_level=noise_level,
    )
    wav_bytes = _tensor_to_wav_bytes(audio_out, SAMPLE_RATE)
    return Response(content=wav_bytes, media_type="audio/wav")


@app.post("/maqam", summary="Maqam-specific generation (LoRA shortcut)")
async def maqam_generate(
    maqam: str = Form(...),
    instrument: str = Form("oud"),
    duration: float = Form(15.0),
    steps: int = Form(100),
    cfg_scale: float = Form(7.0),
    seed: int = Form(-1),
    lora_strength: float = Form(1.0),
):
    prompt = (
        f"Solo {instrument} performance in Maqam {maqam}, "
        f"microtonal, ornamented, traditional Arabic music, close-mic, studio quality"
    )
    if args.lora_ckpt_path:
        model.set_lora_strength(lora_strength)

    audio = model.generate(
        prompt=prompt,
        duration=duration,
        steps=steps,
        cfg_scale=cfg_scale,
        seed=seed,
    )
    wav_bytes = _tensor_to_wav_bytes(audio, SAMPLE_RATE)
    return Response(content=wav_bytes, media_type="audio/wav")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    print(f"\nAPI running at http://{args.host}:{args.port}")
    print(f"Docs at       http://{args.host}:{args.port}/docs\n")
    uvicorn.run(app, host=args.host, port=args.port)
