#!/bin/bash
# Build Ubuntu/Debian .deb. Produces dist/azravibe-recorder_<version>_amd64.deb
# Linux is the only platform we build on this machine, so use the Makefile —
# it needs no cmake and works on any dev box with build-essential.
set -euo pipefail

VERSION="${1:-1.3.3}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$ROOT/dist/deb-root"
OUT="$ROOT/dist"

echo "==> Building Linux binary (v$VERSION)"
make -C "$ROOT" clean
make -C "$ROOT" -j"$(nproc)"

BIN="$ROOT/build/azravibe-recorder"
if [ ! -f "$BIN" ]; then
    echo "error: $BIN not found" >&2
    exit 1
fi

echo "==> Staging package"
rm -rf "$WORK"
mkdir -p "$WORK/DEBIAN"
mkdir -p "$WORK/usr/bin"
mkdir -p "$WORK/usr/share/applications"
mkdir -p "$WORK/usr/share/icons/hicolor/scalable/apps"

install -m 755 "$BIN" "$WORK/usr/bin/azravibe-recorder"
install -m 644 "$ROOT/assets/azravibe-recorder.svg" "$WORK/usr/share/icons/hicolor/scalable/apps/azravibe-recorder.svg"
install -m 644 "$ROOT/assets/azravibe-recorder.desktop" "$WORK/usr/share/applications/azravibe-recorder.desktop"
sed -i 's/@BIN@/azravibe-recorder/' "$WORK/usr/share/applications/azravibe-recorder.desktop"

# Desktop file wants absolute Icon path or themed name on Ubuntu — both work.
cp "$ROOT/packaging/linux/DEBIAN/control" "$WORK/DEBIAN/control"
cp "$ROOT/packaging/linux/DEBIAN/postinst" "$WORK/DEBIAN/postinst"
cp "$ROOT/packaging/linux/DEBIAN/postrm"    "$WORK/DEBIAN/postrm"
sed -i "s/^Version: .*/Version: ${VERSION}/" "$WORK/DEBIAN/control"
chmod 755 "$WORK/DEBIAN/postinst" "$WORK/DEBIAN/postrm"

echo "==> Building .deb"
mkdir -p "$OUT"
dpkg-deb --root-owner-group --build "$WORK" "$OUT/azravibe-recorder_${VERSION}_amd64.deb"
echo "==> Built $OUT/azravibe-recorder_${VERSION}_amd64.deb"
dpkg-deb --info "$OUT/azravibe-recorder_${VERSION}_amd64.deb" | head -20
