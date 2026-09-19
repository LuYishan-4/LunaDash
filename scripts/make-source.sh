#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$project_dir/packaging/arch"
tar -czf "$project_dir/packaging/arch/ludash-1.0.0.tar.gz" \
  --exclude='__pycache__' --exclude='*.pyc' \
  --transform='s,^,ludash-1.0.0/,' -C "$project_dir" \
  CMakeLists.txt .clang-format cmake LICENSE README.md src data qml protocols scripts tests docs packaging/arch/PKGBUILD
