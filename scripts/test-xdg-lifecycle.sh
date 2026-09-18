#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build-wayland}"
runtime_dir="$(mktemp -d)"
socket_name="ludash-xdg-lifecycle"
log_file="${build_dir}/xdg-lifecycle.log"
client_bin="${build_dir}/xdg-lifecycle-client"
protocol_dir="$(pkg-config --variable=pkgdatadir wayland-protocols)"
protocol_xml="${protocol_dir}/stable/xdg-shell/xdg-shell.xml"
header="${build_dir}/xdg-shell-client-protocol.h"
protocol_c="${build_dir}/xdg-shell-client-protocol.c"
pid=""

cleanup() {
  if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
  fi
  rm -rf "${runtime_dir}"
}
trap cleanup EXIT

mkdir -p "${build_dir}"
chmod 700 "${runtime_dir}"

wayland-scanner client-header "${protocol_xml}" "${header}"
wayland-scanner private-code "${protocol_xml}" "${protocol_c}"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror   -I"${build_dir}" tests/wayland/xdg_lifecycle_client.c "${protocol_c}"   $(pkg-config --cflags --libs wayland-client) -o "${client_bin}"

env XDG_RUNTIME_DIR="${runtime_dir}"   WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman   LUDASH_DISABLE_XWAYLAND=1 LUDASH_SKIP_SETUP=1   "${build_dir}/lunadash-compositor" --socket "${socket_name}"   --no-shell --exit-after 15000 >"${log_file}" 2>&1 &
pid=$!

for _ in $(seq 1 120); do
  if [[ -S "${runtime_dir}/${socket_name}" ]]; then
    break
  fi
  if ! kill -0 "${pid}" 2>/dev/null; then
    cat "${log_file}"
    echo "LunaDash exited before the xdg lifecycle client connected" >&2
    exit 1
  fi
  sleep 0.1
done

if [[ ! -S "${runtime_dir}/${socket_name}" ]]; then
  cat "${log_file}"
  echo "LunaDash Wayland socket was not created" >&2
  exit 1
fi

if ! env XDG_RUNTIME_DIR="${runtime_dir}" WAYLAND_DISPLAY="${socket_name}"     "${client_bin}"; then
  cat "${log_file}"
  exit 1
fi

sleep 0.6
if ! kill -0 "${pid}" 2>/dev/null; then
  cat "${log_file}"
  echo "LunaDash crashed during mapped xdg-toplevel teardown/animation" >&2
  exit 1
fi

echo "xdg-toplevel lifecycle and close-animation check passed"
