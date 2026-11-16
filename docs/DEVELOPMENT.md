# Development notes

## Repo layout

```
azravibe-recorder/
├── azravibe-recorder   # launcher (sets LD_LIBRARY_PATH/QT_PLUGIN_PATH for the sandbox sysroot)
├── Makefile            # build / run / smoke / shot / install / uninstall
├── assets/             # icon (SVG) + desktop entry
├── docs/               # screenshot, this file
└── src/
    ├── main.cpp            # entry point, debug env hooks
    ├── MainWindow.*        # window assembly, polling, shortcuts
    ├── Style.h             # design tokens + QSS
    ├── RefreshComboBox.*   # QComboBox with painted chevron, refresh-on-popup
    ├── TitleBar.*          # custom title bar: logo, name, minimize/close, drag
    ├── PreviewWidget.*     # letterboxed live preview + REC/PAUSED badge + toast
    ├── WaveformWidget.*    # scrolling peak-history waveform (real PCM data)
    ├── RecordButton.*      # gradient-ring record/stop button
    ├── PauseButton.*       # pause/resume button
    ├── RecorderController.*# threads, media clock, encode pipeline, handoff atomics
    ├── audio/
    │   ├── AudioDevices.*  # source enumeration via `pactl -f json list sources`
    │   └── AudioCapturer.* # pa_simple blocking read, 48 kHz S16 stereo
    ├── capture/
    │   ├── X11Headers.h    # central X11 include; pushes/pops macros that clash with Qt
    │   ├── ScreenCapturer.*# MIT-SHM grab + XGetImage fallback, window-tracked regions
    │   └── WindowEnumerator.* # EWMH _NET_CLIENT_LIST + RandR monitors
    └── media/
        ├── Encoder.*       # avformat muxer: h264_nvenc/libx264/mpeg4 + aac/mp3/mp2, mp4/mkv
        └── RawPixel.h
```

## Debug env hooks

Both are no-ops in normal use:

- `AZRAVIBE_SHOT=/path/out.png` — render the window to a PNG and exit.
  Use with the offscreen platform for headless screenshots:

  ```bash
  QT_QPA_PLATFORM=offscreen AZRAVIBE_SHOT=shot.png ./azravibe-recorder
  ```

- `AZRAVIBE_SMOKE_SECONDS=N` — auto-start recording with the default
  target/source, stop after N seconds, print the saved path and exit. Used by
  `make smoke` as an end-to-end sanity check (must run on an X11 session for
  the capture part; the GUI can stay offscreen).

## X11 macro clash

X11 exports macros (`None`, `KeyPress`, `Bool`, `Status`, …) that Qt's headers
reuse as identifiers and Qt's `qmetatype.h`/`qtextstream.h` hard-error on.
Rule: **never include Qt after X11 headers in a translation unit**, and include
X11 only through `src/capture/X11Headers.h`, which brackets the includes in
`#pragma push_macro/pop_macro`.

## Threading model

- GUI thread: owns all widgets; polls controller state at ~30 Hz
  (`MainWindow::poll`) — no signals/slots between threads.
- Video thread: ticks the media clock, grabs frames (~30/s), encodes, and
  produces the 512 px preview (skipped while minimized).
- Audio thread: blocking `pa_simple_read` in 10 ms blocks; peaks → waveform
  ring buffer; PCM → AAC.
- Muxer writes are serialized by a mutex inside `Encoder`.
- `mediaUs` is the single clock for video pts, the GUI timer and pause
  freezing. Audio keeps its own sample-index clock that freezes the same way.

## Performance levers

- `SWS_FAST_BILINEAR` for both yuv420p and preview conversions.
- `crf=21`, `p4`/`veryfast`, `bframes=0` (plain MP4-safe pts).
- Preview skipped when minimized (`setPreviewWanted(false)`).
- Pa server buffer ≈ 500 ms so encoder CPU bursts don't drop samples.

## Testing before release

```bash
make clean && make
make smoke                       # writes ~/Videos/Azravibe Recorder/azravibe-*.mp4
QT_QPA_PLATFORM=offscreen ./azravibe-recorder   # or: real X session
```

Verify the output with any player (or `ffprobe`): duration ≈ N seconds,
h264 (NVENC/libx264) 3840×2160 video, AAC 48 kHz stereo.
