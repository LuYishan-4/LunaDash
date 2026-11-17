#!/usr/bin/env bash
# Fetches LunaDash-Shell at the pinned revision (GitHub, then its mirror).
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
destination=${1:?Usage: fetch-shell.sh DESTINATION [REVISION]}
revision=${2:-$(cat "$project_dir/packaging/shell-revision")}

exec bash "$project_dir/scripts/fetch-pinned.sh" LunaDash-Shell \
    https://github.com/LunaDashWM/LunaDash-Shell.git "$destination" "$revision"
