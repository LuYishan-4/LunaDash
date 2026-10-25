#!/usr/bin/env bash
# Fetches SlintLu, the runtime of the LunaDash shell, at the pinned revision.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
destination=${1:?Usage: fetch-slintlu.sh DESTINATION [REVISION]}
revision=${2:-$(cat "$project_dir/packaging/slintlu-revision")}
repository=https://github.com/LunaDashWM/SlintLu.git

[[ $revision =~ ^[0-9a-f]{40}$ ]] || {
    printf 'Invalid SlintLu revision: %s\n' "$revision" >&2
    exit 2
}
[[ ! -e $destination ]] || {
    printf 'Destination already exists: %s\n' "$destination" >&2
    exit 2
}

git -c maintenance.auto=false init --quiet "$destination"
git -C "$destination" remote add origin "$repository"
git -C "$destination" fetch --quiet --no-tags --depth=1 origin "$revision"
git -C "$destination" checkout --quiet --detach FETCH_HEAD
actual=$(git -C "$destination" rev-parse HEAD)
[[ $actual == "$revision" ]] || {
    printf 'Downloaded SlintLu %s, expected %s.\n' "$actual" "$revision" >&2
    exit 1
}
printf 'Fetched SlintLu %s\n' "$actual"
