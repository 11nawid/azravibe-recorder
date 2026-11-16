#!/bin/bash
# Builds the Windows installer tree via windeployqt, bundles FFmpeg DLLs and the
# prebuilt assets/azravibe-recorder.ico, then packages with NSIS.
#   scripts/package-windows.sh <qt6-path> <ffmpeg-dir> <version>
set -euo pipefail

QT6_DIR="${1:?path to qt6 bin dir (e.g. C:/Qt/6.7.2/msvc2022_64)}"
FFMPEG_DIR="${2:?path to ffmpeg root (include/ + lib/ + bin/)}"
VERSION="${3:-1.4.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE="$ROOT/dist/windows-stage"

echo "==> Staging Windows installer (${VERSION})"

# 1) Build
cmake -B "$ROOT/build-win" -S "$ROOT" -G "Ninja" -DCMAKE_BUILD_TYPE=Release \
    -DFFMPEG_DIR="$FFMPEG_DIR"
cmake --build "$ROOT/build-win"

# 2) Stage deployment
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp "$ROOT/build-win/azravibe-recorder.exe" "$STAGE/"

"$QT6_DIR/windeployqt.exe" --release --no-translations "$STAGE/azravibe-recorder.exe"

# Bundle FFmpeg runtime DLLs
cp "$FFMPEG_DIR"/bin/*.dll "$STAGE/" 2>/dev/null || cp "$FFMPEG_DIR"/*.dll "$STAGE/"

# 3) Icon: use the prebuilt assets/azravibe-recorder.ico (shipped in the repo)
install -m 644 "$ROOT/assets/azravibe-recorder.ico" "$STAGE/azravibe-recorder.ico"
echo "==> Staged at $STAGE"
