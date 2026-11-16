# Changelog

All notable changes to this project are documented here.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and the project uses [Semantic Versioning](https://semver.org/).

## [1.2.0] - 2026-11-16

### Added
- **Windows support** (Windows 10/11, x64): DXGI Desktop Duplication for screens and
  PrintWindow for windows; WASAPI loopback for system audio + WASAPI capture for
  microphones; GDI cursor compositing; native `.exe` installer (NSIS).
- **macOS support** (Apple Silicon & Intel): CGDisplayCreateImage fullscreen capture
  and CGWindowList window capture; AVAudioEngine microphone input; native `.dmg`
  (macdeployqt + ad-hoc signature).
- Release pipeline (GitHub Actions) — `v*` tags produce `.deb`, Windows installer
  (`azravibe-recorder-*-setup.exe`) and `.dmg` automatically and attach them to a release.
- CMake build as the primary build system; `Makefile` kept for Linux dev.
- Capture backends unified behind a platform-neutral `CaptureBackend` interface
  (`createCaptureBackend()` / `listCaptureTargets()`), one codebase, one behavior.
- Automated `.deb` control metadata with proper runtime dependencies, icon cache
  refresh in postinst/postrm, and Start Menu shortcut on Windows.

### Changed
- Version numbering → 1.2.0 (cross-platform release).
- Project is no longer Linux-only: same UI, same controls, same output everywhere.

### Known limitations
- macOS v1 records the microphone only — system-audio loopback via Core Audio
  process taps is on the roadmap.
- Windows / macOS builds are produced by CI; functional hard-testing on those
  platforms is in progress (reports welcome).

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
  initial development because tmpfs is wiped on reboot.
- `docs/probe.cpp` (media inspection helper) committed; `docs/screenshot.png` refreshed.

## [1.0.1] - 2026-11-16

### Fixed
- Launcher now falls back to `~/.local/opt/azravibe-recorder/sysroot` when the
  build-time sysroot (in tmpfs) is absent (e.g. after reboot),
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
