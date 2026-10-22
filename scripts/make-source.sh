#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Native plugin examples live under data/plugins; SDK templates live under
# templates/plugins. Keep required inputs strict instead of skipping omissions.
set -- CMakeLists.txt .clang-format cmake LICENSE README.md install.sh src data qml \
  protocols scripts tests docs templates packaging/arch/PKGBUILD flake.nix flake.lock nix
for entry do
  if [ ! -e "$project_dir/$entry" ]; then
    printf 'Missing required source path: %s\n' "$entry" >&2
    exit 1
  fi
done

# A superproject checkout must contain every pinned component before an archive
# is created. The packaging test fixture expands gitlinks into ordinary files.
if [ -e "$project_dir/.git" ] &&
   git -C "$project_dir" ls-files --stage | grep -q '^160000 '; then
  submodules=$(git -C "$project_dir" submodule status --recursive) || {
    echo 'Could not read pinned component status.' >&2
    exit 1
  }
  if printf '%s\n' "$submodules" | grep -Eq '^[+-U]'; then
    echo 'Initialize and checkout every pinned component before packaging.' >&2
    exit 1
  fi
fi

# Stage beside the final archive so a failed tar never replaces a good package.
metadata=$(mktemp -d "$project_dir/packaging/arch/.make-source.XXXXXX")
trap 'rm -rf "$metadata"' EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
# The guided installer records the exact fetched commit before invoking this
# packager. Never borrow a parent directory's Git revision or trust arbitrary
# version-file contents.
if [ -e "$project_dir/.git" ]; then
  revision=$(git -C "$project_dir" rev-parse HEAD)
  if [ -n "$(git -C "$project_dir" status --porcelain --untracked-files=no)" ]; then
    revision="$revision-dirty"
  fi
elif [ -f "$project_dir/.lunadash-revision" ]; then
  revision=$(cat "$project_dir/.lunadash-revision")
  if [ "${#revision}" -ne 40 ] && [ "${#revision}" -ne 46 ] ||
     ! printf '%s\n' "$revision" | grep -Eq '^[0-9a-f]{40}(-dirty)?$'; then
    echo 'Invalid pinned source revision.' >&2
    exit 1
  fi
else
  echo 'Source must be a Git checkout or contain a pinned .lunadash-revision.' >&2
  exit 1
fi
printf '%s\n' "$revision" > "$metadata/.lunadash-revision"
tar -czf "$metadata/ludash-1.0.1a.tar.gz" \
  --exclude='__pycache__' --exclude='*.pyc' --exclude='.git' \
  --transform='s,^,ludash-1.0.1a/,' -C "$project_dir" "$@" \
  -C "$metadata" .lunadash-revision
mv -f -- "$metadata/ludash-1.0.1a.tar.gz" \
  "$project_dir/packaging/arch/ludash-1.0.1a.tar.gz"
