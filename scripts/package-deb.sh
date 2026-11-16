#!/bin/bash
# Build Ubuntu/Debian .deb from the release build tree.
#   scripts/package-deb.sh [version]
# Produces dist/azravibe-recorder_<version>_amd64.deb
set -euo pipefail

VERSION="${1:-1.2.0}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
WORK="$ROOT/dist/deb-root"
OUT="$ROOT/dist"

echo "==> Packaging azravibe-recorder ${VERSION} .deb"

# 1) Build
cmake -B "$BUILD_DIR" -S "$ROOT" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

# 2) Stage
rm -rf "$WORK"
mkdir -p "$WORK/DEBIAN"
mkdir -p "$WORK/usr/bin"
mkdir -p "$WORK/usr/share/azravibe-recorder"
mkdir -p "$WORK/usr/share/applications"
mkdir -p "$WORK/usr/share/icons/hicolor/scalable/apps"

install -m 755 "$BUILD_DIR/azravibe-recorder" "$WORK/usr/bin/azravibe-recorder"
install -m 644 "$ROOT/assets/azravibe-recorder.svg" "$WORK/usr/share/icons/hicolor/scalable/apps/azravibe-recorder.svg"
install -m 644 "$ROOT/assets/azravibe-recorder.desktop" "$WORK/usr/share/applications/azravibe-recorder.desktop"
sed -i "s/@BIN@/azravibe-recorder/" "$WORK/usr/share/applications/azravibe-recorder.desktop"

# 3) Control metadata
cp "$ROOT/packaging/linux/DEBIAN/control" "$WORK/DEBIAN/control"
cp "$ROOT/packaging/linux/DEBIAN/postinst" "$WORK/DEBIAN/postinst"
cp "$ROOT/packaging/linux/DEBIAN/postrm"    "$WORK/DEBIAN/postrm"
sed -i "s/^Version: .*/Version: ${VERSION}/" "$WORK/DEBIAN/control"
chmod 755 "$WORK/DEBIAN/postinst" "$WORK/DEBIAN/postrm"

# 4) Build the .deb
mkdir -p "$OUT"
dpkg-deb --root-owner-group --build "$WORK" "$OUT/azravibe-recorder_${VERSION}_amd64.deb"
echo "==> Built $OUT/azravibe-recorder_${VERSION}_amd64.deb"
