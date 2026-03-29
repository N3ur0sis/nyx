#!/usr/bin/env bash
# Check that all project C source files conform to .clang-format.
# Exits non-zero if any file would be reformatted.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "error: clang-format not found (set CLANG_FORMAT env var)" >&2
    exit 1
fi

FAIL=0

while IFS= read -r -d '' file; do
    # Skip vendored code
    case "$file" in
        */vendor/*) continue ;;
    esac

    if ! "$CLANG_FORMAT" --dry-run --Werror "$file" 2>/dev/null; then
        echo "format: $file needs reformatting"
        FAIL=1
    fi
done < <(find "$ROOT/libs" "$ROOT/tools" -name '*.c' -o -name '*.h' | tr '\n' '\0')

if [ "$FAIL" -eq 0 ]; then
    echo "All source files are correctly formatted."
else
    echo ""
    echo "Run: clang-format -i <file> to fix, or format all:"
    echo "  find libs tools -name '*.c' -o -name '*.h' | grep -v vendor | xargs clang-format -i"
    exit 1
fi
