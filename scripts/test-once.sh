#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUNADASH_TEST_BUILD_DIR:-"$project_dir/build-once"}
shell_source_dir=${LUDASH_SHELL_SOURCE_DIR:-${LUDASH_QML_SOURCE_DIR:-}}

if [ "${XDG_SESSION_TYPE:-}" != wayland ] || [ -z "${WAYLAND_DISPLAY:-}" ]; then
  echo 'Run this one-shot window test from an existing Wayland session.' >&2
  exit 2
fi

if [ -z "$shell_source_dir" ]; then
  shell_source_dir="$build_dir/_deps/LunaDash-Shell"
  expected_revision=$(cat "$project_dir/packaging/shell-revision")
  if [ -d "$shell_source_dir/.git" ]; then
    actual_revision=$(git -C "$shell_source_dir" rev-parse HEAD)
    if [ "$actual_revision" != "$expected_revision" ]; then
      printf 'Existing LunaDash-Shell checkout is %s; expected %s. Remove %s and retry.\n' \
        "$actual_revision" "$expected_revision" "$shell_source_dir" >&2
      exit 1
    fi
  else
    mkdir -p "$(dirname -- "$shell_source_dir")"
    bash "$project_dir/scripts/fetch-shell.sh" "$shell_source_dir"
  fi
fi
if [ ! -f "$shell_source_dir/shell.slint" ] && [ ! -f "$shell_source_dir/shell.qml" ]; then
  printf 'LUDASH_SHELL_SOURCE_DIR does not contain shell.slint: %s\n' "$shell_source_dir" >&2
  exit 1
fi
shell_source_dir=$(CDPATH= cd -- "$shell_source_dir" && pwd)

cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLUDASH_SHELL_SOURCE_DIR="$shell_source_dir"
cmake --build "$build_dir" --parallel "${LUNADASH_TEST_JOBS:-2}"

if command -v slintlu >/dev/null 2>&1 && [ -f "$shell_source_dir/shell.slint" ]; then
  slintlu check "$shell_source_dir"
fi

LUDASH_BUILD_DIR="$build_dir" \
LUDASH_TEST_HOST_WAYLAND=1 \
LUNADASH_DISABLE_FCITX=1 \
LUDASH_GRAPHICS="${LUDASH_GRAPHICS:-gles}" \
  "$project_dir/scripts/test-wayland.sh"

printf '\nOne-shot LunaDash window test passed.\n'
printf 'Log: %s/wayland.log\n' "$build_dir"
printf 'State: %s/wayland-state.json\n' "$build_dir"
