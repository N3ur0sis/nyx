#!/usr/bin/env bash
# Build a release tarball for NYX.
#
# Usage:
#   bash scripts/package-release.sh [version]
#
# If version is omitted, it is extracted from CMakeLists.txt.
# Output: nyx-<version>-linux-<arch>.tar.gz + .sha256 in _release/
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# Resolve version
if [ -n "${1:-}" ]; then
    VERSION="$1"
else
    VERSION=$(sed -n 's/^[[:space:]]*VERSION[[:space:]]\+\([0-9]\+\.[0-9]\+\.[0-9]\+\).*/\1/p' CMakeLists.txt | head -1)
    if [ -z "$VERSION" ]; then
        echo "error: could not extract version from CMakeLists.txt" >&2
        exit 1
    fi
fi

ARCH=$(uname -m)
RELEASE_NAME="nyx-${VERSION}-linux-${ARCH}"
STAGE_DIR="$ROOT/_release/$RELEASE_NAME"

echo "=== Packaging NYX v${VERSION} for linux/${ARCH} ==="

# Clean
rm -rf "$ROOT/_release"
mkdir -p "$STAGE_DIR"

# Build
echo "--- Configuring ---"
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/local

echo "--- Building ---"
cmake --build build -- -j"$(nproc)"

# Install into staging prefix
echo "--- Staging install ---"
DESTDIR="$STAGE_DIR" cmake --install build

# Add extra files to the tarball root
cp LICENSE "$STAGE_DIR/" 2>/dev/null || true
cp README.md "$STAGE_DIR/"
cp CHANGELOG.md "$STAGE_DIR/"
cp scripts/install.sh "$STAGE_DIR/"

echo "--- Creating tarball ---"
cd "$ROOT/_release"
tar czf "${RELEASE_NAME}.tar.gz" "$RELEASE_NAME"

echo "--- Generating checksums ---"
sha256sum "${RELEASE_NAME}.tar.gz" > "${RELEASE_NAME}.tar.gz.sha256"

echo ""
echo "Release artifacts:"
ls -lh "$ROOT/_release/${RELEASE_NAME}".tar.gz*
echo ""
echo "Done."
