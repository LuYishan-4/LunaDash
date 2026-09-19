#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUNADASH_TEST_BUILD_DIR:-"$project_dir/build-once"}

if [ "${XDG_SESSION_TYPE:-}" != wayland ] || [ -z "${WAYLAND_DISPLAY:-}" ]; then
  echo 'Run this one-shot window test from an existing Wayland session.' >&2
  exit 2
fi

cmake -S "$project_dir" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" --parallel "${LUNADASH_TEST_JOBS:-2}"

if command -v qmllint >/dev/null 2>&1; then
  find "$project_dir/qml" -type f -name '*.qml' -exec qmllint -I "$project_dir/qml" '{}' +
fi

LUDASH_BUILD_DIR="$build_dir" \
LUDASH_TEST_HOST_WAYLAND=1 \
LUNADASH_DISABLE_FCITX=1 \
LUDASH_GRAPHICS="${LUDASH_GRAPHICS:-gles}" \
  "$project_dir/scripts/test-wayland.sh"

printf '\nOne-shot LunaDash window test passed.\n'
printf 'Log: %s/wayland.log\n' "$build_dir"
printf 'State: %s/wayland-state.json\n' "$build_dir"
