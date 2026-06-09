# MOTIF (Hackathon Edition)

**AI-powered audio generation plugin for Ableton Live, with a Stable Audio 3 LoRA fine-tuned on Arabic maqam.**

> Repository: `motif_hack` — this is the hackathon proof-of-concept. The name reserves `motif` itself for any productized version.

![platform](https://img.shields.io/badge/platform-macOS-blue) ![status](https://img.shields.io/badge/status-proof%20of%20concept-orange) ![license](https://img.shields.io/badge/license-MIT-green)

> Winner — **Stability AI Challenge**, Music Hackspace × Berklee Hackathon (June 7, 2026)
> Third place — **Ableton Challenge**

MOTIF is an end-to-end pipeline that demonstrates how a fine-tuned generative audio model can serve musical traditions that are systematically underrepresented in commercial training data. The project consists of three components:

1. **A LoRA adapter** trained on 14 Arabic maqam recordings (~20,000 steps, Stable Audio 3 medium) that teaches the base model microtonal pitch structures, traditional ornamentation, and instrument timbres it cannot produce on its own
2. **A local Python API** (FastAPI + Flask bridge) that serves the LoRA-augmented model for inference
3. **A JUCE VST3 plugin** with deep Ableton Live integration via the Extensions SDK, allowing audio generation to flow directly into a new track inside the DAW

The intent is both technical and political: AI music tools claim cultural universality but are trained on a commercial archive that is ~94% Western. MOTIF makes that gap visible by demonstrating, side by side, what the base model produces from a maqam prompt versus what the LoRA-augmented model produces.

---

## Team

- **Thiago Santos** — model training, FastAPI server, system architecture
- **Chris Powers** — JUCE plugin, Ableton Extension SDK integration
- **Jad Al-Masri** — maqam dataset, microtonal music expertise, concept leader
- **Rithik (Ricky) Kundu** — UI/UX designer, maqam modern track production
- **Michael Tomasi** — UI/UX designer

Model training assistance: **CJ Carr** (Dadabots, Stable Audio 3 co-author).
Built at the Music Hackspace Hackathon, Berklee College of Music, Boston — June 6–7, 2026.

---

## Architecture

```
┌─────────────────────────┐
│ Ableton Live 12 Beta    │
│  ┌───────────────────┐  │       ┌──────────────────────┐
│  │ MOTIF VST3        │──┼──HTTP─│ Flask Bridge (8080)  │
│  │ (JUCE)            │  │       │  server.py           │
│  └───────────────────┘  │       └──────────┬───────────┘
│           │             │                  │
│           │ writes      │                  │ HTTP
│           ▼             │                  ▼
│  /tmp/AINewPlug/        │       ┌──────────────────────┐
│   ai_output.wav         │       │ FastAPI (8765)       │
│   ai_output_ready.txt   │       │  stable_audio_       │
│           │             │       │  server.py           │
│           ▼             │       │                      │
│  ┌───────────────────┐  │       │  ↓ loads             │
│  │ Ableton Extension │  │       │  Stable Audio 3      │
│  │ (TypeScript/Node) │  │       │  medium + maqam LoRA │
│  │  → new audio track│  │       └──────────────────────┘
│  └───────────────────┘  │
└─────────────────────────┘
```

The three components communicate over `localhost`. The plugin is a normal VST3 that can run in any DAW; the Ableton-specific auto-track feature requires Live 12 Beta because it uses the Extensions SDK (currently in early access).

---

## Repository structure

```
motif_hack/
├── README.md                       ← this file (overview + setup)
├── LICENSE                         ← MIT
├── lora/
│   ├── README.md                   ← LoRA training reference
│   ├── maqam-step=10000-epoch=714.safetensors
│   └── dataset_captions/           ← T3-tier captions used for training
├── server/
│   ├── stable_audio_server.py      ← FastAPI server (port 8765)
│   ├── server.py                   ← Flask bridge for JUCE (port 8080)
│   ├── requirements.txt
│   └── README.md                   ← API contract + endpoint docs
├── plugin/
│   ├── AINewPlug/                  ← JUCE plugin sources + .jucer
│   ├── AbletonExtension/           ← TypeScript Ableton extension
│   ├── Readme.pdf                  ← detailed plugin guide (Chris Powers)
│   └── README.md                   ← plugin quick reference
└── docs/
    └── (architecture diagrams, screenshots)
```

---

## Quick start

You need three servers running and the VST3 installed. Detailed instructions for each piece are in their respective READMEs.

### 1. Clone

```bash
git clone https://github.com/astronarta22/motif_hack.git
cd motif_hack
```

### 2. Set up the LoRA + Stable Audio runtime

```bash
# Clone Stable Audio 3 in a sibling folder (not inside this repo)
cd ..
git clone https://github.com/Stability-AI/stable-audio-3
cd stable-audio-3

# Install dependencies via uv
uv sync --extra lora

# Authenticate with HuggingFace (you must accept the medium model license at
# https://huggingface.co/stabilityai/stable-audio-3-medium first)
uv run python -c "from huggingface_hub import login; login()"

# Place the LoRA checkpoint
mkdir -p lora_checkpoints
cp ../motif_hack/lora/maqam-step=10000-epoch=714.safetensors lora_checkpoints/
```

> **Note on LoRA training**: the 10,000-step checkpoint shipped in this repo was trained on a Google Colab H100 GPU using the 14-file maqam dataset and T3-tier captions in `lora/dataset_captions/`. The training code itself lives in the official [Stable Audio 3 repository](https://github.com/Stability-AI/stable-audio-3) (`scripts/train_lora.py`); we do not duplicate it here. See `lora/README.md` for the exact training command and rationale.

### 3. Start the three servers (in three terminals)

**Terminal 1 — FastAPI inference server** (loads the model + LoRA, listens on 8765):
```bash
cd stable-audio-3
uv run python /path/to/motif_hack/server/stable_audio_server.py \
    --model medium \
    --lora-ckpt-path lora_checkpoints/maqam-step=10000-epoch=714.safetensors \
    --lora-strength 1.0
```

**Terminal 2 — Flask bridge** (JUCE ↔ FastAPI adapter, listens on 8080):
```bash
cd motif_hack/server
pip3 install -r requirements.txt
python3 server.py
```

**Terminal 3 — Ableton Extension** (creates new tracks from generated audio):

> For this proof of concept, start this server **after** the plugin has been loaded in the Ableton Live interface.

```bash
cd motif_hack/plugin/AbletonExtension
npm install
npm start
```


### 4. Install the VST3 plugin

See [`plugin/README.md`](plugin/README.md) for a quick build reference, or [`plugin/Readme.pdf`](plugin/Readme.pdf) for Chris Powers' detailed guide covering Projucer setup, Xcode build steps, code signing, and troubleshooting. Short version:

1. Open `plugin/AINewPlug/AINewPlug.jucer` in Projucer
2. Set Global JUCE path, click "Save and Open in IDE"
3. Build in Xcode (`Cmd+B`)
4. Copy + ad-hoc sign the VST3:
   ```bash
   cp -r plugin/AINewPlug/Builds/MacOSX/build/Debug/AINewPlug.vst3 ~/Library/Audio/Plug-Ins/VST3/
   sudo codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/AINewPlug.vst3
   ```

### 5. Run in Ableton Live 12 Beta

1. Open Ableton Live 12 Beta
2. Load `AINewPlug` VST3 on an audio track
3. Drag a WAV file into the plugin's drop zone (the "browse files" button is not working on the proof of concept)
4. Choose **Append** (inpaint a region) or **Restyle** (full reimagining)
4.1 For **Append** mode, visually select the segment of the input file you want the model to use as reference. We recommend starting after 3 seconds. The prompt is optional in this mode.
4.2 For **Restyle**, select the "Creativity" level (0 is closer to source, 1 is closer to prompt). Experiments showed that values around 0.15 maintained the source melody, successfully changing the instruments while preserving maqam microtonality.  
5. Type a prompt (e.g. `"Solo qanun in Maqam, microtonal melody, traditional Arabic"`)
6. Click **Generate**
7. After ~30–60 seconds (medium model on Apple Silicon MPS), a new **AI Generated** track appears automatically with the result

---

## Status: proof of concept

This is a hackathon-quality prototype with deliberate limitations that the team plans to address in subsequent work:

- **Temp files everywhere.** The plugin writes WAVs to `$TMPDIR/AINewPlug/` and the Ableton Extension polls a trigger file. This works on a single machine but is fragile across sessions and not suitable for production. A proper IPC mechanism (named pipes, WebSocket, or direct Extension API audio handoff) is the next step.
- **`npm start` per session.** The Ableton Extension is a TypeScript process started manually each time. Future versions should bundle it as part of the Live Extension itself rather than running externally.
- **Ad-hoc code signing.** The VST3 is signed with a self-signed certificate (`codesign --sign -`), which restricts it to Ableton Live 12 Beta and other lenient DAWs. A paid Apple Developer certificate is required for shipping a Release build.
- **Ableton Live 12 Beta required.** The auto-track feature depends on the Extensions SDK, which is in early access and only available in the Beta. The plugin functions as a standalone or in any DAW for the generation step itself, but the new-track behavior is Live 12 Beta only.
- **macOS only.** JUCE itself is cross-platform, but the Extension SDK integration and the Apple Silicon MPS inference path are macOS-specific. A Windows port is feasible but not currently scoped.
- **No streaming.** Generation is one-shot (~30–90s round-trip). Real-time/live processing would require a completely different model and architecture.
- **LoRA training scale.** The 14-file dataset is sufficient for a proof of concept but not for a robust production model, given the breadth of instruments and scales in Arabic maqam. A serious version would need on the order of 100–500 representative recordings.

---

## Acknowledgements

- **JUCE** — the C++ framework that makes cross-platform audio plugin development possible
- **Ableton Extensions SDK** — early access beta SDK that enabled the auto-track integration
- **Stable Audio 3** (Stability AI) — the open-weight model architecture this work builds on
- **CJ Carr** (Dadabots) — LoRA training guidance and the Dadabots Underfit tooling
- **Music Hackspace × Berklee** — for hosting the hackathon where this was built
- **Claude (Anthropic)** — AI pair programmer used throughout development

---

## License

MIT License — see [LICENSE](LICENSE) for details.
