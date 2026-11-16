# Azravibe Recorder

**Azravibe Recorder** is a minimal, aesthetic screen recorder for Linux (X11).
It records the full screen, a single monitor, or a single window — with audio
from any PulseAudio / PipeWire source — to H.264 + AAC in an `.mp4` (or
Matroska when the fallback encoders kick in).

Built in C++17, Qt 6 (Widgets), FFmpeg (libav) and PulseAudio simple API.
No Electron, no bundled browser, no nonsense.

![Azravibe Recorder](docs/screenshot.png)

## Features

- One giant, honest **Record** button — click to start, click again to stop
- **Pause / Resume** mid-recording (file duration tracks the frozen pause)
- **Live preview** of the exact screen / monitor / window being recorded
- **Real waveform** of the captured audio (peak history, violet → cyan)
- **Cursor baked into the recording** — toggle it off with the ✓ *Cursor* checkmark
- **4K UHD output** (3840×2160) — lanczos upscale of lower-res sources; encoded
  with **NVENC** when available (`h264_nvenc`), libx264 otherwise
- **Choose your output folder** — the bar under the dropdowns shows the exact
  folder videos go to; click the folder button to change it, click the path to
  open it in your file manager (stored in `QSettings`, survives restarts)
- Two dropdowns and nothing else: **what to record** + **audio source**
  - Targets: whole display, individual RandR monitors, any regular window
  - Sources: every PulseAudio/PipeWire source — monitors (system audio) and inputs (mic)
- **Minimize keeps recording** — the capture pipeline is fully independent of the window
- Close (✕) finalizes the file and quits the whole app
- Automatic file naming: `azravibe-YYYYMMDD-HHMMSS.mp4` in the output folder
- A/V stays in sync on a wall-clock media timeline; pauses don't leave gaps
- Low footprint: SHM-based capture, double-buffered frames, ~160 MB RSS typical
- If the target window closes or quits mid-recording, the file is finalized cleanly

## Requirements

Runtime:
- X11 session (works under Xwayland too)
- PulseAudio or PipeWire (pipewire-pulse is supported — `pactl` must be on PATH)
- Qt 6.4+ base, FFmpeg 6 libs (libavformat/libavcodec/libswscale/libswresample/libavutil)
  - libavcodec built with **libx264** strongly recommended (falls back to MPEG-4 otherwise)

Build (Debian/Ubuntu):

```bash
sudo apt install build-essential \
  qt6-base-dev libx11-dev libxext-dev libxrandr-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
  libpulse-dev libxfixes-dev pkg-config
```

## Build & run

```bash
make            # build → build/azravibe-recorder
./azravibe-recorder
```

On machines without system Qt dev packages, `make` auto-detects the sandbox
sysroot at `/tmp/opencode/sysroot` (used by the author's CI sandbox). To force
system lookup: `make SYSROOT=`.

### Install

```bash
make install    # installs ~/.local/bin/azravibe-recorder(+bin), icon, .desktop
```

## Controls

| Control | Action |
|---|---|
| Record button (right) | Start / stop recording |
| Pause button (left of it) | Pause / resume (only while recording) |
| `Ctrl+R` | Toggle record/stop |
| `Ctrl+Space` | Toggle pause/resume |
| `Ctrl+Q` | Finalize & quit |
| Title bar ✕ | Finalize & quit (same as close) |
| Title bar — | Minimize (**recording continues**) |
| Drag title bar | Move window |

## Smoke test (headless-ish)

```bash
make smoke      # offscreen run: records ~3 s, writes a real .mp4, prints the path
make shot       # renders the window to /tmp/opencode/azravibe-shot.png
```

`AZRAVIBE_SMOKE_SECONDS=N` and `AZRAVIBE_SHOT=/path.png` are honored by the
binary for CI; `AZRAVIBE_SHOT` renders with the offscreen QPA platform.

## Where do videos go?

By default: **`~/Videos/Azravibe Recorder/azravibe-YYYYMMDD-HHMMSS.mp4`**.
The row under the dropdowns shows the exact folder; the folder button opens a
picker to change it, and clicking the path opens it in your file manager.
4K UHD · NVENC (or libx264) · AAC audio. Settings live in
`~/.config/azravibe/azravibe-recorder.conf` (`outputDir`, `showCursor`).

## How it works

```
┌──────────┐   XShm/XGetImage   ┌────────────┐
│ X server │ ─────────────────► │  Capturer  │
└──────────┘                    └─────┬──────┘
                                      │ BGRX frames (30 tick)
        ┌─────────────────────────────┼──────────────────┐
        ▼                             ▼                  ▼
┌───────────────┐            ┌────────────────┐   preview (512 px,
│ swscale→yuv420p│            │ capture→Qt     │   RGB888, only while
│ libx264 → muxer│            │ PreviewWidget  │   window is visible)
└──────┬────────┘            └────────────────┘
       │ av_interleaved_write_frame
┌──────▼────────┐
│  mkv/mp4 file │
└──────▲────────┘
       │ av_interleaved_write_frame
┌──────┴────────┐
│ swr→fltp, AAC │
└──────▲────────┘
       │ int16 PCM @48 kHz stereo (10 ms blocks)
┌──────┴────────┐
│ pa_simple_read│   ← dev = chosen source (or server default)
└───────────────┘
```

Timestamps come from a single wall-clock **media clock** (skipped while paused),
so video `pts`, audio sample indices and the on-screen timer can never drift
apart — pausing freezes all three.

## Performance notes

- Capture reuses a resident MIT-SHM segment (`XShmGetImage`); requires no extra copies.
- Preview renders at 512 px wide, skipped entirely while minimized/hidden.
- NVENC H.264 (`p4`, cq≈21, no B-frames; falls back to libx264 veryfast CRF 21) → valid plain MP4 muxing without a dts shuffle.
- 1920×1080 at 30 fps or 4K at ~25–30 fps (NVENC, RTX 3050 measured).
- Measured RSS ≈ 155–160 MB while idle/encoding (Qt + FFmpeg; NVENC offloads the heavy lifting).

## Troubleshooting

- **System audio is silent / track shorter than video**
  The chosen source (usually a `monitor`) only streams when the server runs.
  PipeWire may starve idle monitors — pick the microphone or any input source
  instead, or ensure audio is actually playing while recording.
- **`pactl` not found**
  Install `pulseaudio-utils` (or `pipewire-pulse`). The UI falls back to short
  source listing, and to the server default source if nothing is listed.
- **Weird window captured**
  The window is captured by cropping the root window region, so it records
  exactly what you see (including overlapping windows on top of it).
- **Monitor selection**
  Multi-head uses RandR monitors ("Monitor 2 · primary", etc.). Falls back to
  one "Entire display" entry.

## Development

Environment hooks (not for end users):

```bash
AZRAVIBE_SHOT=/tmp/x.png QT_QPA_PLATFORM=offscreen ./azravibe-recorder
AZRAVIBE_SMOKE_SECONDS=3 QT_QPA_PLATFORM=offscreen ./azravibe-recorder
```

Debug RSS/timings: see `docs/DEVELOPMENT.md` (or just `top -p $(pgrep azravibe)`).

## Versioning

This repo uses Git with semantic tags (`v1.0.0`, …). See `CHANGELOG.md`.

## License

MIT — see `LICENSE`.
