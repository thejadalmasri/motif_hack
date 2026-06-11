# MOTIF (Hackathon Edition)

**AI-powered audio generation plugin for Ableton Live, with a Stable Audio 3 LoRA fine-tuned on Arabic maqam.**

> Repository: `motif_hack` — this is the hackathon proof-of-concept. The name reserves `motif` itself for any productized version.

![platform](https://img.shields.io/badge/platform-macOS-blue) ![status](https://img.shields.io/badge/status-proof%20of%20concept-orange) ![license](https://img.shields.io/badge/license-MIT%20%2B%20NOTICE-green)

> Winner — **Stability AI Challenge**, Music Hackspace × Berklee Hackathon (June 7, 2026)
> Third place — **Ableton Challenge**

MOTIF is an end-to-end pipeline that demonstrates how a fine-tuned generative audio model can serve musical traditions that are systematically underrepresented in commercial training data. The project consists of three components:

1. **A LoRA adapter** trained on Arabic maqam recordings (owned by @thejadalmasri) that teaches Stable Audio 3 medium microtonal pitch structures, traditional ornamentation, and instrument timbres the base model cannot produce on its own. **The trained weights, dataset, and caption methodology are proprietary Motif IP — see [`NOTICE`](NOTICE).**
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
├── LICENSE                         ← MIT (code only)
├── NOTICE                          ← license scope: what MIT does NOT cover
├── lora/
│   └── README.md                   ← stub; weights/dataset/captions are proprietary
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
    └── (audio examples, screenshots — to be populated)
```

---

## Quick start

You need three servers running and the VST3 installed. Detailed instructions for each piece are in their respective READMEs.

> **Note on the LoRA**: this repository contains the open-source plumbing (plugin, server, extension) but does **not** ship the trained LoRA weights, dataset, or caption methodology. To reproduce the full end-to-end demo, you need access to those proprietary assets — contact the team via [motiiif.com](https://motiiif.com). The code below assumes you have placed a compatible `.safetensors` LoRA checkpoint in `lora_checkpoints/`.

### 1. Clone

```bash
git clone https://github.com/astronarta22/motif_hack.git
cd motif_hack
```

### 2. Set up the Stable Audio runtime

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

# Place your LoRA checkpoint in lora_checkpoints/
mkdir -p lora_checkpoints
# (copy your .safetensors LoRA file into lora_checkpoints/ here)
```

### 3. Start the three servers (in three terminals)

**Terminal 1 — FastAPI inference server** (loads the model + LoRA, listens on 8765):
```bash
cd stable-audio-3
uv run python /path/to/motif_hack/server/stable_audio_server.py \
    --model medium \
    --lora-ckpt-path lora_checkpoints/your_lora.safetensors \
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
- **Dataset scale.** The hackathon LoRA was trained on a small proof-of-concept dataset. A robust production model requires substantially more representative recordings, given the breadth of instruments and scales in Arabic maqam. The dataset and training methodology are proprietary Motif IP.

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

Source code in this repository is licensed under the [MIT License](LICENSE).

The trained model weights, dataset, dataset captions, and training methodology
are **proprietary Motif IP, all rights reserved**, and are **not** covered by
the MIT License. See [`NOTICE`](NOTICE) for the full scope.
