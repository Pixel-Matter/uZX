#!/bin/bash
set -euo pipefail

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
output_dir="${1:-$repo_root/docs/images/psg-visualization}"
sheet="${2:-}"
cmake --preset default
cmake --build build --target uZXTests --parallel 4
UZX_PSG_GUIDE_OUTPUT="$output_dir" UZX_PSG_GUIDE_SHEET="$sheet" build/src/uZXTests_artefacts/Debug/uZXTests PsgVisualGuide
