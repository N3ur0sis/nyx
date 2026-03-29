#!/usr/bin/env bash
# NYX installer -- installs from a local extracted tarball or fetches
# the latest release from GitHub.
#
# Usage:
#   # From an extracted tarball directory:
#   sudo bash install.sh
#
#   # From anywhere (downloads latest release):
#   curl -fsSL https://raw.githubusercontent.com/N3ur0sis/nyx/main/scripts/install.sh | sudo bash
#
# Options:
#   --prefix <path>    Install prefix (default: /usr/local)
#   --no-man           Skip man page installation
#   --no-caps          Skip setting Linux capabilities on binaries
#   --help             Show this help
set -euo pipefail

PREFIX="/usr/local"
INSTALL_MAN=1
SET_CAPS=1
GITHUB_REPO="N3ur0sis/nyx"

usage() {
    echo "NYX Installer"
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --prefix <path>    Install prefix (default: /usr/local)"
    echo "  --no-man           Skip man page installation"
    echo "  --no-caps          Skip setting Linux capabilities"
    echo "  --help             Show this help"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --prefix)   PREFIX="$2"; shift 2 ;;
        --no-man)   INSTALL_MAN=0; shift ;;
        --no-caps)  SET_CAPS=0; shift ;;
        --help)     usage; exit 0 ;;
        *)          echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

BIN_DIR="$PREFIX/bin"
MAN_DIR="$PREFIX/share/man/man8"

# Determine source: are we inside an extracted tarball?
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ -d "$SCRIPT_DIR/usr/local/bin" ]; then
    SRC_BIN="$SCRIPT_DIR/usr/local/bin"
    SRC_MAN="$SCRIPT_DIR/usr/local/share/man/man8"
elif [ -d "$SCRIPT_DIR/bin" ]; then
    SRC_BIN="$SCRIPT_DIR/bin"
    SRC_MAN="$SCRIPT_DIR/share/man/man8"
else
    echo "=== Downloading latest NYX release ==="
    TMPDIR=$(mktemp -d)
    trap 'rm -rf "$TMPDIR"' EXIT

    LATEST_URL=$(curl -fsSL "https://api.github.com/repos/$GITHUB_REPO/releases/latest" \
        | grep -oP '"browser_download_url":\s*"\K[^"]*linux[^"]*\.tar\.gz(?=")')

    if [ -z "$LATEST_URL" ]; then
        echo "error: could not find a Linux release tarball" >&2
        exit 1
    fi

    echo "Downloading: $LATEST_URL"
    curl -fsSL "$LATEST_URL" -o "$TMPDIR/nyx.tar.gz"
    tar xzf "$TMPDIR/nyx.tar.gz" -C "$TMPDIR"

    EXTRACTED=$(find "$TMPDIR" -maxdepth 1 -mindepth 1 -type d | head -1)
    SRC_BIN="$EXTRACTED/usr/local/bin"
    SRC_MAN="$EXTRACTED/usr/local/share/man/man8"

    if [ ! -d "$SRC_BIN" ]; then
        SRC_BIN="$EXTRACTED/bin"
        SRC_MAN="$EXTRACTED/share/man/man8"
    fi
fi

echo "=== Installing NYX ==="
echo "Prefix: $PREFIX"

# Install binaries
mkdir -p "$BIN_DIR"
for bin in "$SRC_BIN"/nyx*; do
    [ -f "$bin" ] || continue
    name=$(basename "$bin")
    echo "  install: $BIN_DIR/$name"
    install -m 755 "$bin" "$BIN_DIR/$name"
done

# Install man pages
if [ "$INSTALL_MAN" -eq 1 ] && [ -d "$SRC_MAN" ]; then
    mkdir -p "$MAN_DIR"
    for man in "$SRC_MAN"/*.8; do
        [ -f "$man" ] || continue
        name=$(basename "$man")
        echo "  install: $MAN_DIR/$name"
        install -m 644 "$man" "$MAN_DIR/$name"
    done
fi

# Set capabilities (optional, requires root)
if [ "$SET_CAPS" -eq 1 ] && command -v setcap >/dev/null 2>&1; then
    if [ "$(id -u)" -eq 0 ]; then
        echo ""
        echo "=== Setting capabilities ==="
        for bin in nyx-pingsweep nyx-arpspoof nyx-macspoof; do
            target="$BIN_DIR/$bin"
            if [ -x "$target" ]; then
                setcap cap_net_raw,cap_net_admin=eip "$target" 2>/dev/null && \
                    echo "  setcap: $target (cap_net_raw,cap_net_admin)" || \
                    echo "  warn: could not set capabilities on $target"
            fi
        done
        if [ -x "$BIN_DIR/nyx" ]; then
            setcap cap_net_raw,cap_net_admin=eip "$BIN_DIR/nyx" 2>/dev/null && \
                echo "  setcap: $BIN_DIR/nyx (cap_net_raw,cap_net_admin)" || \
                echo "  warn: could not set capabilities on nyx"
        fi
    else
        echo ""
        echo "Note: run as root to set Linux capabilities on binaries."
        echo "  This avoids needing sudo for raw socket tools."
    fi
fi

echo ""
echo "=== NYX installed successfully ==="

# Check PATH
if ! echo "$PATH" | tr ':' '\n' | grep -qx "$BIN_DIR"; then
    echo ""
    echo "Add $BIN_DIR to your PATH if not already present:"
    echo "  export PATH=\"$BIN_DIR:\$PATH\""
fi

echo ""
echo "Get started:"
echo "  nyx              # interactive shell"
echo "  nyx version      # verify installation"
echo "  man nyx          # read the manual"
