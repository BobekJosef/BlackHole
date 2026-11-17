#!/usr/bin/env sh
# Formatting check and static analysis, as CI runs them.
#   scripts/lint.sh            check only (exit 1 on findings)
#   scripts/lint.sh --fix      reformat in place first
# Needs clang-format and clang-tidy (18 or newer) and a configured build with compile_commands.json
# (any preset; build/debug by default, override with BUILD_DIR).
set -eu
cd "$(dirname "$0")/.."
build="${BUILD_DIR:-build/debug}"
sources=$(git ls-files --cached --others --exclude-standard 'src/*.cpp' 'src/*.hpp' 'tests/*.cpp')

if [ "${1:-}" = "--fix" ]; then
    echo "$sources" | xargs clang-format -i
fi

echo "clang-format..."
echo "$sources" | xargs clang-format --dry-run --Werror

echo "clang-tidy ($build)..."
if [ ! -f "$build/compile_commands.json" ]; then
    echo "no $build/compile_commands.json: configure a preset first (cmake --preset debug)" >&2
    exit 2
fi
report=$(echo "$sources" | grep '\.cpp$' | xargs -P "$(nproc 2>/dev/null || echo 4)" -n 1 clang-tidy -p "$build" --quiet 2>/dev/null || true)
if [ -n "$report" ]; then
    echo "$report"
    exit 1
fi
echo "clean"
