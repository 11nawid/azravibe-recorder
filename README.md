<div align="center">

# Azravibe Recorder
### Free, Open-Source 4K Screen Recorder — Windows · macOS · Linux

**The aesthetic, zero-bloat screen recorder that just works — one binary per OS.**
Record your screen, any monitor, or any window — in crisp **4K UHD** with
real-time audio monitoring, a live waveform, hardware NVENC encoding and a
frameless, human-designed interface.

<img src="docs/screenshot.png" alt="Azravibe Recorder — minimal 4K Linux screen recorder UI" width="620"/>

[![License](https://img.shields.io/badge/license-MIT-a78bfa?style=flat-square)](LICENSE)
[![Version](https://img.shields.io/badge/version-v1.4.0-22d3ee?style=flat-square)](CHANGELOG.md)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20X11-2a2a35?style=flat-square)](https://x.org/)
[![Language](https://img.shields.io/badge/language-C%2B%2B17-8b5cf6?style=flat-square)](https://isocpp.org/)
[![Qt](https://img.shields.io/badge/toolkit-Qt%206-41cd52?style=flat-square)](https://www.qt.io/)
[![Stars](https://img.shields.io/github/stars/11nawid/azravibe-recorder?color=ffd166&style=flat-square)](https://github.com/11nawid/azravibe-recorder/stargazers)
[![Forks](https://img.shields.io/github/forks/11nawid/azravibe-recorder?color=4ade80&style=flat-square)](https://github.com/11nawid/azravibe-recorder/network/members)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen?style=flat-square&logo=github)](CONTRIBUTING.md)

⭐ **If this helps you, tap the star — it genuinely helps the project grow.** ⭐

</div>

> ⚠️ **Platform status:** Linux (X11) is fully tested. Windows 10/11 and macOS builds
> are produced by CI on every release but are **not yet validated on real hardware** —
> treat those builds as beta and report issues on the issue tracker.

---

---

## Why another screen recorder?

Most Linux screen recorders are either Electron wrappers weighing half a
gigabyte, or two-knob CLI tools with no soul. **Azravibe Recorder is neither.**
It is a single native binary — C++17 + Qt 6 — that gives you a **beautiful,
productive** recording workflow in ~160 MB of RAM:

- 👁️ **Live preview even when not recording** — watch any display, monitor or window before you hit record; pick a target from the dropdowns and see it instantly
- 🎥 **Real 4K UHD (3840×2160)** recording with lanczos upscale, NVENC on your GPU
- 🌊 **Live, real audio waveform** — actual PCM peaks, not a fake animation
- 🖱️ **Cursor baked in** — pointer visible in recordings (toggle with ✓ Cursor)
- ⏸️ **Pause/Resume** — wall-clock timeline keeps audio & video perfectly in sync
- 🖥️ **Three capture targets** — entire display · per-monitor (RandR) · any window
- 🎚️ **Hot-swap audio source mid-recording** — mic ↔ system audio, live
- 🗂️ **Choose your output folder** — one glance shows exactly where files go
- 🔔 **Minimize keeps recording** — close the window, it never stops your capture
- 🚫 **Zero Electron, zero cloud, zero account** — completely local & private

### Keywords
`open source screen recorder` · `4k screen recorder linux` · `linux screencast` ·
`x11 screen recorder` · `windows screen recorder` · `mac screen recorder` · `ubuntu screen recorder` · `screen recorder linux mint` ·
`record desktop with audio` · `free screen recorder linux` · `c++ screen recorder` ·
`qt screen recorder` · `azravibe recorder`

---

## Download

| Platform | File | Notes |
|---|---|---|
| Windows 10/11 (64-bit) | **`azravibe-recorder-1.4.0-setup.exe`** | One-click installer, Start Menu + desktop shortcut |
| Linux (Debian / Ubuntu / Mint, X11) | **`azravibe-recorder_1.4.0_amd64.deb`** | `sudo dpkg -i …` — menu entry included |
| macOS (Apple Silicon & Intel) | **`azravibe-recorder-1.4.0.dmg`** | Drag to Applications; allow in Privacy → Screen Recording |

Get them from the [Releases](https://github.com/11nawid/azravibe-recorder/releases) page.

## Quick start — from source

```bash
git clone https://github.com/11nawid/azravibe-recorder.git
cd azravibe-recorder
```

**Linux (Debian/Ubuntu/Mint):**

```bash
sudo apt install build-essential cmake pkg-config \
  qt6-base-dev libx11-dev libxext-dev libxrandr-dev libxfixes-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
  libpulse-dev
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/azravibe-recorder
```

**Windows (Visual Studio 2022 + Qt 6.7 + prebuilt FFmpeg):**

```powershell
# Qt 6.7.2 MSVC 2022 64-bit from qt.io
cmake -B build -G "Visual Studio 17 2022" -A x64 `
      -DFFMPEG_DIR=C:/path/to/ffmpeg-full-shared
cmake --build build --config Release
```

**macOS (Homebrew):**

```bash
brew install qt@6 ffmpeg cmake
cmake -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --build build
```

**Install:**

```bash
make install              # Linux user install (~/.local/bin + .desktop)
./scripts/package-deb.sh  # build Ubuntu/Debian .deb for release
```

> NVIDIA users: `h264_nvenc` is auto-detected and used at 4K for free;
> otherwise everything falls back to libx264 automatically.

## Controls

| Control | Action |
|---|---|
| Record button | Start / stop recording |
| Pause button | Pause / resume (only while recording) |
| `Ctrl+R` / `Ctrl+Space` / `Ctrl+Q` | Record · Pause · Quit |
| Audio dropdown | Swap source **while recording** (mic ↔ system audio live) |
| Title bar **—** | Minimize (**recording continues**) |
| Title bar **✕** | Stop recording, finalize file, quit |

**Output:** `~/Videos/Azravibe Recorder/azravibe-YYYYMMDD-HHMMSS.mp4` (folder is
configurable in-app and persisted in `QSettings`).

## What it records

| Track | Codec | Container |
|---|---|---|
| Video | H.264 (NVENC or libx264), 3840×2160 | MP4 (faststart) |
| Audio | AAC 48 kHz stereo, 160 kbps | MP4 |

### Platform support
- **Linux (X11)** — fully tested and supported (Linux Mint / Ubuntu family, Cinnamon tested).
- **Windows 10/11** — builds ship from CI on every `v*` tag, but have **not been run on real Windows hardware yet**. Treat as beta.
- **macOS (Apple Silicon & Intel)** — same: builds ship, hardware validation pending. Microphone capture only; system-audio loopback is on the roadmap.
- **Wayland** — not supported yet (needs the PipeWire/XDG portal path).

## Roadmap

- [ ] Wayland capture path (PipeWire ScreenCast portal)
- [ ] Global hotkeys for record/pause from anywhere
- [ ] Per-source gain & mic monitoring in the waveform
- [ ] Snapshot / GIF export buttons
- [ ] Localization (Persian, German, Spanish, …)

Have an idea? [Open an issue](https://github.com/11nawid/azravibe-recorder/issues) — PRs welcome.

## Support the project 💜

Azravibe Recorder is free and open source, built and maintained by one person.
If it saved your time:

- ⭐ **Star** this repo (it costs nothing, means a lot)
- 📣 **Share** it with friends, your Linux group, or on YouTube/Reddit
- ☕ **Help / donate / sponsor** — message me directly on Instagram:
  **[@1n1.nawid](https://www.instagram.com/1n1.nawid/)** or find me on GitHub:
  **[11nawid](https://github.com/11nawid)**

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) — it covers the build, the smoke test
(`make smoke` — headless, writes a real .mp4), coding style and how to submit PRs.

## License

MIT — see [LICENSE](LICENSE). Do almost anything you want with it; attribution appreciated.

<sub>Keywords: azravibe-recorder · screen-recorder · linux · x11 · ubuntu · mint · 4k · nvenc · qt6 · c++ · ffmpeg · open-source</sub>
