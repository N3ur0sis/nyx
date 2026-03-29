#!/usr/bin/env bash
# docs-check.sh -- Validate NYX documentation quality
#
# Checks:
#   1. Every shipped tool has a docs page
#   2. Every shipped tool has a man page
#   3. Internal Markdown links resolve to existing files
#   4. workflow.schema.json is valid JSON
#   5. Example workflows referenced in docs exist
#
# Exit code: 0 if all checks pass, 1 if any fail.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ERRORS=0

fail() {
    echo "FAIL: $1" >&2
    ERRORS=$((ERRORS + 1))
}

pass() {
    echo "  ok: $1"
}

# ---------- 1. Tool docs coverage ----------
echo "=== Tool documentation coverage ==="

TOOLS=$(cd "$ROOT/bin" 2>/dev/null && ls nyx-* 2>/dev/null | sed 's/^nyx-//' || true)
if [ -z "$TOOLS" ]; then
    echo "  (no binaries in bin/, checking source instead)"
    TOOLS=$(find "$ROOT/tools/phobos" -mindepth 1 -maxdepth 1 -type d -printf '%f\n' 2>/dev/null | sort || true)
fi

for tool in $TOOLS; do
    [ "$tool" = "run" ] && continue
    if [ -f "$ROOT/docs/tools/$tool.md" ]; then
        pass "docs/tools/$tool.md exists"
    else
        fail "docs/tools/$tool.md missing for shipped tool '$tool'"
    fi
done

# ---------- 2. Man page coverage ----------
echo "=== Man page coverage ==="

for tool in $TOOLS; do
    [ "$tool" = "run" ] && continue
    found=0
    for mandir in "$ROOT"/tools/phobos/*/man "$ROOT"/tools/nyx*/man; do
        [ -f "$mandir/nyx-$tool.8" ] && found=1 && break
        [ -f "$mandir/nyx-$tool.1" ] && found=1 && break
    done
    if [ "$found" -eq 1 ]; then
        pass "man page for nyx-$tool found"
    else
        fail "man page for nyx-$tool missing"
    fi
done

# nyx master
if [ -f "$ROOT/tools/nyx/man/nyx.1" ]; then
    pass "man page for nyx found"
else
    fail "man page for nyx missing"
fi

# nyx-run
if [ -f "$ROOT/tools/nyx-run/man/nyx-run.8" ]; then
    pass "man page for nyx-run found"
else
    fail "man page for nyx-run missing"
fi

# ---------- 3. Internal link checking ----------
echo "=== Internal Markdown link checking ==="

while IFS= read -r -d '' mdfile; do
    dir=$(dirname "$mdfile")
    links=$(grep -oP '\]\(\K[^)#]+\.md(?=[)#])' "$mdfile" 2>/dev/null || true)
    [ -z "$links" ] && continue
    while read -r link; do
        case "$link" in
            http*|mailto*) continue ;;
        esac
        target="$dir/$link"
        if [ -f "$target" ]; then
            : # ok
        else
            fail "$mdfile -> $link (target does not exist)"
        fi
    done <<< "$links"
done < <(find "$ROOT/docs" -name '*.md' -print0)

# Check README links to docs/
readme_links=$(grep -oP '\]\(\Kdocs/[^)#]+\.md(?=[)#])' "$ROOT/README.md" 2>/dev/null || true)
if [ -n "$readme_links" ]; then
    while read -r link; do
        if [ -f "$ROOT/$link" ]; then
            pass "README -> $link"
        else
            fail "README -> $link (target does not exist)"
        fi
    done <<< "$readme_links"
fi

# ---------- 4. Schema validation ----------
echo "=== Schema validation ==="

SCHEMA="$ROOT/docs/reference/workflow.schema.json"
if [ -f "$SCHEMA" ]; then
    if python3 -c "import json; json.load(open('$SCHEMA'))" 2>/dev/null; then
        pass "workflow.schema.json is valid JSON"
    else
        fail "workflow.schema.json is not valid JSON"
    fi
else
    fail "workflow.schema.json not found"
fi

# ---------- 5. Example workflow files ----------
echo "=== Example workflow files ==="

if [ -f "$ROOT/workflows/net-discovery.json" ]; then
    if python3 -c "import json; json.load(open('$ROOT/workflows/net-discovery.json'))" 2>/dev/null; then
        pass "workflows/net-discovery.json is valid JSON"
    else
        fail "workflows/net-discovery.json is not valid JSON"
    fi
else
    fail "workflows/net-discovery.json not found"
fi

# ---------- Summary ----------
echo ""
if [ "$ERRORS" -eq 0 ]; then
    echo "All documentation checks passed."
    exit 0
else
    echo "$ERRORS documentation check(s) failed."
    exit 1
fi
