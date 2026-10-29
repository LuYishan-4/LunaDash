#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

# Native plugin examples live under data/plugins; lunadash-create-plugin writes new
# plugin projects itself. Keep required inputs strict instead of skipping omissions.
set -- CMakeLists.txt .clang-format cmake LICENSE README.md install.sh src data \
  protocols scripts tests docs packaging/arch/PKGBUILD \
  packaging/shell-revision packaging/slintlu-revision flake.nix flake.lock nix
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
shell_source=${LUDASH_SHELL_SOURCE_DIR:-${LUDASH_QML_SOURCE_DIR:-}}
if [ -z "$shell_source" ]; then
  shell_source="$metadata/LunaDash-Shell"
  bash "$project_dir/scripts/fetch-shell.sh" "$shell_source"
else
  if [ ! -d "$shell_source" ]; then
    printf 'LunaDash-Shell source is unavailable: %s\n' "$shell_source" >&2
    exit 1
  fi
  shell_source=$(CDPATH= cd -- "$shell_source" && pwd)
fi
if [ ! -f "$shell_source/shell.slint" ] ||
   [ ! -x "$shell_source/install.sh" ]; then
  printf 'LunaDash-Shell source is unavailable or incomplete: %s\n' "$shell_source" >&2
  exit 1
fi
mkdir -p "$metadata/LunaDash-Shell-package"
# The Slint shell's installer would compile it with whatever SlintLu the host has; the
# package builds and checks it against the pinned SlintLu instead.
if [ -f "$shell_source/shell.slint" ]; then
  bash "$shell_source/install.sh" --prefix "$metadata/stage" --no-check
else
  bash "$shell_source/install.sh" --prefix "$metadata/stage"
fi
cp -a "$metadata/stage/share/lunadash/shell/." "$metadata/LunaDash-Shell-package/"
# SlintLu, the shell runtime, at the pinned revision (sources only; built by the package).
slintlu_source=${LUDASH_SLINTLU_SOURCE_DIR:-}
if [ -z "$slintlu_source" ]; then
  slintlu_source="$metadata/SlintLu"
  bash "$project_dir/scripts/fetch-slintlu.sh" "$slintlu_source"
fi
if [ ! -f "$slintlu_source/Cargo.toml" ]; then
  printf 'SlintLu source is unavailable or incomplete: %s\n' "$slintlu_source" >&2
  exit 1
fi
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
ludash_version=$(sed -n 's/^    set(LUDASH_RELEASE_VERSION "\([^"]*\)")$/\1/p' "$project_dir/cmake/LunaDashMain.cmake" | head -n1)
if [ -z "$ludash_version" ]; then
    echo 'Unable to read LUDASH_RELEASE_VERSION from cmake/LunaDashMain.cmake.' >&2
    exit 1
fi
tar -czf "$metadata/ludash-${ludash_version}.tar.gz" \
  --exclude='__pycache__' --exclude='*.pyc' --exclude='.git' \
  --transform="s,^,ludash-${ludash_version}/," -C "$project_dir" "$@" \
  -C "$metadata" .lunadash-revision
tar -czf "$metadata/ludash-shell.tar.gz" \
  --transform='s,^\./,LunaDash-Shell/,' \
  -C "$metadata/LunaDash-Shell-package" .
tar -czf "$metadata/ludash-slintlu.tar.gz" \
  --exclude='.git' --exclude='target' \
  --transform="s,^\\.,SlintLu," -C "$slintlu_source" .
mv -f -- "$metadata/ludash-${ludash_version}.tar.gz" \
  "$project_dir/packaging/arch/ludash-${ludash_version}.tar.gz"
mv -f -- "$metadata/ludash-shell.tar.gz" \
  "$project_dir/packaging/arch/ludash-shell.tar.gz"
mv -f -- "$metadata/ludash-slintlu.tar.gz" \
  "$project_dir/packaging/arch/ludash-slintlu.tar.gz"
