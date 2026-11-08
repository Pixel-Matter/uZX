#!/usr/bin/env bash
# Builds zxtune123, the command-line player/converter that turns tracker modules
# (PT3, STC, ASC, ...) into PSG register dumps, from source on macOS.
#
#     scripts/build_zxtune123.sh     # prints the path of the built binary
#
# zxtune publishes no macOS CLI binary, and release r5112 needs three fixes to
# build with current Apple clang. They are applied below and are idempotent, so
# re-running only rebuilds what changed. Needs Homebrew boost (brew install boost).
# ZXTUNE_DIR overrides the checkout location, ZXTUNE_TAG the release.
set -euo pipefail

TAG=${ZXTUNE_TAG:-r5112}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIR=${ZXTUNE_DIR:-$ROOT/.cache/tools/zxtune}
BREW=$(brew --prefix)

[ -d "$BREW/include/boost" ] || { echo "boost not found: brew install boost" >&2; exit 1; }
[ -d "$DIR/.git" ] || git clone --depth 1 --branch "$TAG" https://github.com/vitamin-caig/zxtune.git "$DIR"

# shared_ptr::unique() was removed in C++20
sed -i '' -E 's/([A-Za-z_]+)\.unique\(\)/(\1.use_count() == 1)/g' "$DIR/src/core/plugins/archives/raw_supp.cpp"
# fmt's consteval format strings fail to compile on current clang
sed -i '' -e 's/^#    define FMT_CONSTEVAL consteval$/#    define FMT_CONSTEVAL/' -e '/^#    define FMT_HAS_CONSTEVAL$/d' \
    "$DIR/3rdparty/fmt/include/fmt/core.h"
# Homebrew headers must come last, or the vendored libxmp picks up Homebrew's xmp.h.
# Passing arch= instead would demand a prebuilt.dir.
printf 'darwin.cxx.flags += -idirafter %s/include\nlibraries.dirs.darwin += %s/lib\n' "$BREW" "$BREW" > "$DIR/variables.mak"

make -C "$DIR/apps/zxtune123" platform=darwin release=1 system.zlib=1 -j"$(sysctl -n hw.ncpu)" >&2
echo "$DIR/bin/darwin/release/zxtune123"
