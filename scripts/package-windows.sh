#!/bin/bash
# Builds the Windows installer tree via windeployqt, bundles FFmpeg DLLs,
# generates the .ico from the SVG icon, then packages with NSIS.
#   scripts/package-windows.sh <qt6-path> <ffmpeg-dir> <version>
set -euo pipefail

QT6_DIR="${1:?path to qt6 bin dir (e.g. C:/Qt/6.7.2/msvc2022_64)}"
FFMPEG_DIR="${2:?path to ffmpeg root (include/ + lib/ + bin/)}"
VERSION="${3:-1.2.0}"
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

# 3) Icon (.ico from the SVG via any rasterizer available)
if command -v magick >/dev/null 2>&1; then
    magick -background transparent "$ROOT/assets/azravibe-recorder.svg" -define icon:auto-resize=256,128,64,48,32,16 "$STAGE/azravibe-recorder.ico"
elif command -v rsvg-convert >/dev/null 2>&1 && command -v icotool >/dev/null 2>&1; then
    for SZ in 16 32 48 64 128 256; do
        rsvg-convert -w $SZ "$ROOT/assets/azravibe-recorder.svg" -o "/tmp/azravibe-$SZ.png"
    done
    icotool -c -o "$STAGE/azravibe-recorder.ico" /tmp/azravibe-*.png
else
    echo "warning: no SVG rasterizer found; installer will miss its icon"
fi
cp "$ROOT/assets/azravibe-recorder.desktop" "$STAGE/azravibe-recorder.desktop" || true
echo "==> Staged at $STAGE"
