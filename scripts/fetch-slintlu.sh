#!/usr/bin/env bash
# Fetches SlintLu, the runtime of the LunaDash shell, at the pinned revision
# (GitHub, then its mirror).
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
destination=${1:?Usage: fetch-slintlu.sh DESTINATION [REVISION]}
revision=${2:-$(cat "$project_dir/packaging/slintlu-revision")}

exec bash "$project_dir/scripts/fetch-pinned.sh" SlintLu \
    https://github.com/LunaDashWM/SlintLu.git "$destination" "$revision"
