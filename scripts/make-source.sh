#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Native plugin examples live under data/plugins; SDK templates live under
# templates/plugins. Keep required inputs strict instead of skipping omissions.
set -- CMakeLists.txt .clang-format cmake LICENSE README.md src data qml \
  protocols scripts tests docs templates packaging/arch/PKGBUILD
for entry do
  if [ ! -e "$project_dir/$entry" ]; then
    printf 'Missing required source path: %s\n' "$entry" >&2
    exit 1
  fi
done

# Stage beside the final archive so a failed tar never replaces a good package.
metadata=$(mktemp -d "$project_dir/packaging/arch/.make-source.XXXXXX")
trap 'rm -rf "$metadata"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
revision=$(git -C "$project_dir" rev-parse HEAD)
if [ -n "$(git -C "$project_dir" status --porcelain --untracked-files=no)" ]; then
  revision="$revision-dirty"
fi
printf '%s\n' "$revision" > "$metadata/.lunadash-revision"
tar -czf "$metadata/ludash-1.0.1a.tar.gz" \
  --exclude='__pycache__' --exclude='*.pyc' \
  --transform='s,^,ludash-1.0.1a/,' -C "$project_dir" "$@" \
  -C "$metadata" .lunadash-revision
mv -f -- "$metadata/ludash-1.0.1a.tar.gz" \
  "$project_dir/packaging/arch/ludash-1.0.1a.tar.gz"
