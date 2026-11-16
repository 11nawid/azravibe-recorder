# Contributing to Azravibe Recorder

Thanks for helping make the nicest screen recorder on Linux even better 💜

## Ground rules

- Be kind. No harassment, no gatekeeping.
- Keep the app **minimal**. If a feature adds a knob instead of removing one,
  expect pushback — that's the project's whole personality.
- Linux first. X11 stays supported; Wayland contributions are very welcome.

## Development setup

```bash
git clone https://github.com/11nawid/azravibe-recorder.git
cd azravibe-recorder
sudo apt install build-essential \
  qt6-base-dev libx11-dev libxext-dev libxrandr-dev libxfixes-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
  libpulse-dev pkg-config
make            # build ./build/azravibe-recorder
make smoke      # 3-second headless recording → writes a real .mp4
make shot       # offscreen screenshot of the UI
```

Read `docs/DEVELOPMENT.md` for the architecture, threading model and the X11
macro-clash gotcha you'll hit if you add X11 includes to any TU.

## Pull requests

1. Branch off `main`: `feat/<short-name>` or `fix/<short-name>`.
2. Keep commits small and in imperative mood (`fix: …`, `feat: …`).
3. `make clean && make` must pass. Run `make smoke` once against your X session.
4. Update `CHANGELOG.md` (add to *Unreleased*).
5. Screenshots for UI changes live in the PR, small and dark-mode friendly.

## Releases

Maintainer tags `vX.Y.Z`. SemVer: breaking → major, features → minor, fixes → patch.

## Want to help without code?

- Star the repo, share it, write about it, make a 60-second demo video.
- Report bugs with exact steps + output of `make smoke`.
- Translate strings (see Roadmap → Localization).
