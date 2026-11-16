#!/bin/bash
# macOS: build, make .app bundle with macdeployqt, wrap into .dmg.
#   scripts/package-macos.sh [version]
set -euo pipefail

VERSION="${1:-1.3.4}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
QT_PREFIX="${QT_PREFIX:-$(brew --prefix qt@6 2>/dev/null || brew --prefix qt)}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
APP_NAME="Azravibe Recorder"
APP_PATH="$BUILD_DIR/${APP_NAME}.app"

echo "==> Packaging macOS bundle (${VERSION}) with ${QT_PREFIX}"

cmake -B "$BUILD_DIR" -S "$ROOT" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX;$QT_PREFIX/lib/cmake" \
    -DFFMPEG_DIR="${FFMPEG_DIR:-/opt/homebrew/opt/ffmpeg}"
cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.ncpu)"

APP_PATH="${AZRAVIBE_APP:-$BUILD_DIR/azravibe-recorder.app}"
if [ ! -d "$APP_PATH" ]; then
    echo "error: $APP_PATH not found (expected MACOSX_BUNDLE output)" >&2
    exit 1
fi

"$QT_PREFIX/bin/macdeployqt" "$APP_PATH" -verbose=1

# Ad-hoc sign so Gatekeeper at least recognizes the bundle (user still needs to allow it)
codesign --force --deep --no-strict -s - "$APP_PATH" || true

# Create .dmg via create-dmg if available, else hdiutil
OUT_DIR="$ROOT/dist"
mkdir -p "$OUT_DIR"
DMG_PATH="$OUT_DIR/azravibe-recorder-${VERSION}.dmg"

if command -v create-dmg >/dev/null 2>&1; then
    create-dmg "$DMG_PATH" "$APP_PATH" 2>/dev/null || true
fi

if [ ! -f "$DMG_PATH" ]; then
    UD="$(mktemp -d)"
    cp -R "$APP_PATH" "$UD/${APP_NAME}.app"
    hdiutil create -volname "${APP_NAME}" -srcfolder "$UD" -ov -format UDZO "$DMG_PATH"
    rm -rf "$UD"
fi

echo "==> Built $DMG_PATH"
