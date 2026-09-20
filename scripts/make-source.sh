#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$project_dir/packaging/arch"
metadata=$(mktemp -d)
trap 'rm -rf "$metadata"' EXIT HUP INT TERM
revision=$(git -C "$project_dir" rev-parse HEAD)
if [ -n "$(git -C "$project_dir" status --porcelain --untracked-files=no)" ]; then
  revision="$revision-dirty"
fi
printf '%s\n' "$revision" > "$metadata/.lunadash-revision"
tar -czf "$project_dir/packaging/arch/ludash-1.0.0.tar.gz" \
  --exclude='__pycache__' --exclude='*.pyc' \
  --transform='s,^,ludash-1.0.0/,' -C "$project_dir" \
  CMakeLists.txt .clang-format cmake LICENSE README.md src data qml protocols scripts tests docs examples templates packaging/arch/PKGBUILD \
  -C "$metadata" .lunadash-revision
