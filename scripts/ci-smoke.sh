#!/usr/bin/env bash
# CI smoke test -- verifies that built binaries are present, linked
# correctly, and respond to non-privileged invocations.
#
# Root-requiring tools cannot be fully exercised in CI.  We verify
# binary existence and test any non-root paths (e.g. portscan connect mode).
set -euo pipefail

BIN=./bin
FAIL=0

pass() { printf "  \033[32mPASS\033[0m  %s\n" "$*"; }
fail() { printf "  \033[31mFAIL\033[0m  %s\n" "$*"; FAIL=1; }

echo "=== NYX CI smoke tests ==="

# --- nyx master ---
if "$BIN/nyx" version | grep -qE '^nyx [0-9]+\.[0-9]+\.[0-9]+'; then
    pass "nyx version"
else
    fail "nyx version"
fi

if "$BIN/nyx" info >/dev/null 2>&1; then
    pass "nyx info"
else
    fail "nyx info"
fi

# --- nyx-run ---
if "$BIN/nyx-run" --help 2>&1 | grep -qi 'workflow'; then
    pass "nyx-run --help"
else
    fail "nyx-run --help"
fi

if "$BIN/nyx-run" --version 2>&1 | grep -qE '[0-9]+\.[0-9]+'; then
    pass "nyx-run --version"
else
    fail "nyx-run --version"
fi

# --- tool binaries exist and are executable ---
for tool in pingsweep portscan macspoof arpspoof; do
    bin_name="nyx-$tool"
    if [ -x "$BIN/$bin_name" ]; then
        pass "$bin_name binary exists"
    else
        fail "$bin_name binary not found"
    fi
done

# --- portscan JSON mode (connect mode, no root needed) ---
output=$(timeout 10 "$BIN/nyx-portscan" -J -t 127.0.0.1 -p 1 2>/dev/null || true)
if echo "$output" | grep -q '"nyx"'; then
    pass "nyx-portscan JSON envelope"
else
    fail "nyx-portscan JSON envelope"
fi

echo ""
if [ "$FAIL" -eq 0 ]; then
    echo "All smoke tests passed."
else
    echo "Some smoke tests failed."
    exit 1
fi
