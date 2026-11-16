# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/).

## [1.1.1] - 2026-11-16

### Documentation
- Publication-ready README: SEO-friendly title/description/keywords, badges,
  support & donation links (Instagram @1n1.nawid), publishing checklist.
- `CONTRIBUTING.md`, `.github/FUNDING.yml` and issue templates added.
- Native links now point at `github.com/11nawid/azravibe-recorder`.

## [1.1.0] - 2026-11-16

### Added
- **4K UHD output** (3840×2160) with lanczos upscaling of lower-resolution
  sources; encoded with **NVENC** (`h264_nvenc`, preferred) or libx264 fallback.
- **Cursor capture** — the live mouse cursor is composited into recorded
  frames (libXFixes), toggleable with the new ✓ *Cursor* checkmark in the UI.
- **Output location bar** — shows the current save folder; click the folder
  icon to pick a different output directory, click the path to open it in your
  file manager. Persisted in QSettings.
- Measured ~28 fps at 3840×2160 on the development machine (RTX 3050).

### Fixed
- Build system now defaults to the durable sysroot at
  `~/.local/opt/azravibe-recorder/sysroot` instead of the tmpfs path used during
  initial development.
- `docs/probe.cpp` (media inspection helper) committed; `docs/screenshot.png` refreshed.

## [1.0.1] - 2026-11-16

### Fixed
- Launcher now falls back to `~/.local/opt/azravibe-recorder/sysroot` when the
  build-time sysroot at `/tmp/opencode/sysroot` is absent (e.g. after reboot),
  so installed/desktop launches keep working on machines without system Qt dev
  packages.

## [1.0.0] - 2026-11-16

### Added
- Initial public release.
- X11 screen/window capture via MIT-SHM with `XGetImage` fallback.
- Full display, per-monitor (RandR) and per-window recording targets.
- PulseAudio/PipeWire source selection (monitors + inputs) with 48 kHz S16 stereo capture.
- H.264 (libx264, CRF 21, veryfast, no B-frames) + AAC, MP4 (+faststart) output.
- Matroska + MPEG-4/MP2 fallback path when libx264/AAC are unavailable.
- Wall-clock media timeline; A/V sync holds across pause/resume.
- Minimal frameless Qt 6 UI: live preview, real-time waveform, two dropdowns,
  record/pause transport controls, minimize/close in the title bar.
- "Saved — <file>" toast, REC/PAUSED badge with elapsed time overlay.
- Closing the window finalizes the recording and quits.
- Auto-finalize when the target window disappears mid-recording.
- `make smoke` / `make shot` developer hooks (`AZRAVIBE_SMOKE_SECONDS`, `AZRAVIBE_SHOT`).
- Desktop integration via `make install` (icon + .desktop file).

[1.0.0] - 2026-11-16 — first tagged release.
