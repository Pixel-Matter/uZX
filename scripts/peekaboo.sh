#!/bin/sh
# Run the pinned macOS UI driver without changing the app or installing global tools.
set -eu

if [ "$(uname -s)" != Darwin ]; then
    echo 'Peekaboo requires macOS 15 or later.' >&2
    exit 1
fi

version=4.9.7
case "$(uname -m)" in
    arm64) arch=arm64; sha=147a81a9b31c1b7b7bd5f720674ad7de71951698138d379679678a9ccfdd0a38 ;;
    x86_64) arch=x86_64; sha=b9abefba42b23fda37e1c682e2493a6aa403b6c72b0eefa9773bea517960cb58 ;;
    *) echo 'Unsupported macOS architecture.' >&2; exit 1 ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cache="$root/.cache/peekaboo/$version-$arch"

if [ ! -x "$cache/peekaboo" ]; then
    mkdir -p "$(dirname -- "$cache")"
    staging=$(mktemp -d "$cache.XXXXXX")
    trap 'rm -rf "$staging"' EXIT
    trap 'exit 1' HUP INT TERM
    archive="peekaboo-macos-$arch"
    curl --fail --location --silent --show-error --retry 2 \
        "https://github.com/openclaw/Peekaboo/releases/download/v$version/$archive.tar.gz" \
        --output "$staging/release.tar.gz"
    printf '%s  %s\n' "$sha" "$staging/release.tar.gz" | shasum -a 256 --check >&2
    tar -xzf "$staging/release.tar.gz" -C "$staging"
    test -x "$staging/$archive/peekaboo"
    mv "$staging/$archive" "$cache"
    rm -rf "$staging"
    trap - EXIT HUP INT TERM
fi

exec "$cache/peekaboo" "$@"
