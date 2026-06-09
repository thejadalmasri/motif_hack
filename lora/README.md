# MOTIF — LoRA training

This folder contains the trained maqam LoRA adapter and the captions used to train it. The training code itself is not included here — it lives in the official [Stable Audio 3 repository](https://github.com/Stability-AI/stable-audio-3) as `scripts/train_lora.py`. This document explains how to use the shipped checkpoint and how to reproduce the training.

---

## Files

- `maqam-step=10000-epoch=714.safetensors` — the trained LoRA adapter (36 MB, rank 32, DoRA-rows adapter type)
- `dataset_captions/` — the 14 T3-tier text captions paired with Jad Al-Masri's maqam audio recordings. The audio files themselves are not redistributed; contact the team if you need them.

The 10,000-step checkpoint is the one used in the winning hackathon demo. We also trained 4,000-step and 20,000-step variants; the 10,000-step version had the best balance between learning the maqam character and preserving generation flexibility.

---

## Using the shipped LoRA

The LoRA is loaded by the FastAPI server at startup. See `../server/README.md` for the full command. The short version:

```bash
uv run python stable_audio_server.py \
    --model medium \
    --lora-ckpt-path /path/to/maqam-step=10000-epoch=714.safetensors \
    --lora-strength 1.0
```

`--lora-strength` is a float between 0.0 and 1.0:
- `0.0` — LoRA fully disabled, equivalent to running the base model
- `1.0` — LoRA fully applied (used for the demo)
- Intermediate values blend base and LoRA

---

## Reproducing the training

### Dataset

14 WAV files of Arabic and Turkish maqam performances, each paired with a `.txt` caption file of the same base name. The dataset spans 9 maqamat (Nahawand, Bayati, Rast, Hijaz, Hijaz-Kar, Saba, Siga, Kurd, plus modulations between them) and 7 instruments (ney, qanun, kawala, baglama, zurna, strings, oud).

Caption strategy: we tested three tiers of caption detail (T1 = casual layperson description, T2 = musically precise but concise, T3 = exhaustive technical description with bar counts and cent values for neutral intervals). T3 captions produced noticeably better results during training and are the ones shipped in `dataset_captions/`.

Example T3 caption (Nahawand on ney):

> An Arabic Ney performing solo a short melodic movement in Maqam Nahawand. In the first half, starting on A, it's rhythmic in feeling, decorated with slurred triplet phrases which balance out the rhythmic component. In the second half, the melody consists of a descending melodic pattern, slower in rhythm, with slurs to enhance smoothness and emotional appeal. Small grace notes appear before each main note. The ney's melody returns to F middle range at the end. Close-mic, studio quality.

### Training environment

- Google Colab Pro with H100 (or A100) GPU
- Stable Audio 3 official repository, commit at time of training: `9b6be18`
- Python 3.10, `uv sync --extra lora`

### Training command

```bash
uv run python scripts/train_lora.py \
    --model medium-base \
    --data_dir /path/to/maqam_dataset \
    --rank 32 \
    --adapter_type dora-rows \
    --steps 10000 \
    --checkpoint_every 2000 \
    --batch_size 4 \
    --logger none \
    --num_workers 4
```

Wall-clock time on H100: approximately 3 hours including periodic inpaint demo generation. Final `mse_masked_loss` was around 0.10 (from a baseline of ~0.88 at step 0).

### Converting Lightning checkpoint to safetensors

PyTorch Lightning saves `.ckpt` files. To produce a `.safetensors` file loadable by the inference code:

```python
import torch, json
from safetensors.torch import save_file

ckpt = torch.load("lora_checkpoints/epoch=714-step=10000.ckpt", map_location="cpu")
save_file(
    ckpt["state_dict"],
    "maqam-step=10000-epoch=714.safetensors",
    metadata={"lora_config": json.dumps(ckpt["lora_config"])},
)
```

---

## Design choices and what to know if extending this

**Why rank 32 with DoRA-rows adapter type.** Rank 32 is on the higher end for a 14-file dataset, but maqam tradition encodes subtle pitch information (neutral intervals at ~37–47 cents above the nearest semitone) that needs sufficient parameter capacity to learn. DoRA-rows decomposes the LoRA matrices into magnitude and direction components and learns them separately; it's more expressive than vanilla LoRA at the same rank and the H100 budget supported it easily.

**Why 14 files is enough for a demo but not for production.** The model is clearly learning the maqam character with this dataset, but it is also memorizing the specific recordings and instruments. A production-quality LoRA for any tradition would need on the order of 100–500 representative recordings, with deliberate variation in performers, instruments, recording conditions, and stylistic interpretation.

**On microtonality.** Stable Audio 3 base is trained on 12-TET (the equal-tempered chromatic scale) and produces tempered pitches by default. The LoRA does not change the underlying representation of pitch in the model — it shifts the distribution toward producing the microtonal intervals more often, but it cannot guarantee precise cent values. For applications that require exact tuning (e.g. ethnomusicological research), a post-processing pitch-correction step would be appropriate.

**On the T3 caption tier.** The captions include exact cent values (e.g. "A# +46 cents") even though the model cannot read those numerically. We included them because (a) they were free metadata that future tokenizers might exploit, and (b) they served as documentation for the team and reviewers. The musical character — instrument names, ornament types, modulation paths — was likely the main signal the model picked up on.
