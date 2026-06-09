# MOTIF — JUCE Plugin + Ableton Extension

This folder contains the JUCE VST3 plugin (built with the JUCE 8 framework) and the Ableton Live 12 Beta Extension that creates new audio tracks when the plugin finishes generating.

**Built and maintained by Chris Powers** with contributions from Thiago Santos (audio I/O bug fixes, slider behavior, server integration).

> Full build instructions, requirements, and project structure are in `Readme.pdf` (also in this folder).

---

## Quick reference

### Required versions
- macOS (Apple Silicon or Intel)
- JUCE 8.0
- Xcode 14+
- Node.js 24.14.1+
- Ableton Live 12 Beta (for the auto-track feature)

### Build the VST3

```bash
cd plugin/AINewPlug
# Open AINewPlug.jucer in Projucer
# Set Global JUCE path, save and open in IDE
# Build in Xcode (Cmd+B) in Debug mode

cp -r Builds/MacOSX/build/Debug/AINewPlug.vst3 ~/Library/Audio/Plug-Ins/VST3/
sudo codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/AINewPlug.vst3
```

### Set up the Ableton Extension

```bash
cd plugin/AbletonExtension
npm install
```

Create `.env` with:
```
EXTENSION_HOST_PATH=/Applications/Ableton Live 12 Beta.app/Contents/Helpers/ExtensionHost/ExtensionHostNodeModule.node
```

Then open Live 12 Beta and load `AINewPlug` on an audio track.

### Run

```bash
cd plugin/AbletonExtension
npm start
```
---

## Source structure (when present)

```
plugin/
├── AINewPlug/
│   ├── AINewPlug.jucer
│   └── Source/
│       ├── PluginProcessor.h
│       ├── PluginProcessor.cpp
│       ├── PluginEditor.h
│       └── PluginEditor.cpp
└── AbletonExtension/
    ├── package.json
    ├── tsconfig.json
    └── src/
        └── extension.ts
```

For complete instructions on requirements, build process, code signing, and troubleshooting, see [`Readme.pdf`](Readme.pdf).
