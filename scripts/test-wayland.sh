#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build"}
runtime_dir=$(mktemp -d)
chmod 700 "$runtime_dir"
mkdir -p "$runtime_dir/config"
export XDG_CONFIG_HOME="$runtime_dir/config"
export LUDASH_SKIP_SETUP=1
export QT_FORCE_STDERR_LOGGING=1
rm -f "$build_dir/wayland-state.json" "$build_dir/wayland.log"
trap 'find "$runtime_dir" -depth -delete' EXIT

test_shell_args=""
if [ "${LUDASH_TEST_NO_SHELL:-0}" = 1 ]; then
  test_shell_args="--no-shell"
fi

test_demo_args=""
if [ "${LUDASH_TEST_DEMO:-0}" = 1 ]; then
  test_demo_args="--demo"
fi

if [ "${LUDASH_TEST_HOST_WAYLAND:-0}" = 1 ]; then
  case "${WAYLAND_DISPLAY:-}" in
    /*) host_wayland=$WAYLAND_DISPLAY ;;
    ?*) host_wayland=${XDG_RUNTIME_DIR:?Host XDG_RUNTIME_DIR is required}/$WAYLAND_DISPLAY ;;
    *) echo 'A running host Wayland session is required.' >&2; exit 1 ;;
  esac
  session_status=0
  env XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$host_wayland" \
    WLR_BACKENDS=wayland LUDASH_DISABLE_XWAYLAND=1 \
    "$build_dir/lunadash-compositor" $test_shell_args $test_demo_args \
      --socket ludash-test --exit-after 7000 \
      --state "$build_dir/wayland-state.json" \
      >"$build_dir/wayland.log" 2>&1 || session_status=$?
else
  session_status=0
  env XDG_RUNTIME_DIR="$runtime_dir" \
      WLR_BACKENDS=headless WLR_HEADLESS_OUTPUTS=1 WLR_RENDERER=pixman \
      QT_QUICK_BACKEND=software QSG_RHI_BACKEND=software \
      LUDASH_DISABLE_XWAYLAND=1 \
      "$build_dir/lunadash-compositor" $test_shell_args $test_demo_args \
        --socket ludash-test --exit-after 7000 \
        --state "$build_dir/wayland-state.json" \
        >"$build_dir/wayland.log" 2>&1 || session_status=$?
fi

cat "$build_dir/wayland.log"
if [ "$session_status" -ne 0 ]; then
  echo "LunaDash session exited with status $session_status" >&2
  exit "$session_status"
fi
if [ ! -f "$build_dir/wayland-state.json" ]; then
  echo "LunaDash did not write wayland-state.json" >&2
  exit 2
fi
export LUDASH_TEST_DEMO="${LUDASH_TEST_DEMO:-0}"
python3 - "$build_dir/wayland-state.json" <<'PY'
import json, os, sys
state = json.load(open(sys.argv[1]))
assert not state.get("processFailure", True), state
assert state["input"]["backend"] == "wlroots", state["input"]
assert state["input"]["qtInput"] is False, state["input"]
assert state["input"]["seatProtocolVersion"] >= 5, state["input"]
assert state["input"]["dataDeviceProtocolVersion"] >= 3, state["input"]
clients = [c for c in state["clients"] if not c.get("desktop")]
if os.environ.get("LUDASH_TEST_DEMO") == "1":
    assert len(clients) >= 2, state
    assert all(c["mapped"] for c in clients), clients
    assert any(c["visible"] for c in clients), clients
    assert all(c["width"] > 0 and c["height"] > 0 for c in clients if c["visible"]), clients
display = state.get("display", {})
assert display.get("width", 0) > 0 and display.get("height", 0) > 0, display
print("wlroots session passed:", len(clients), "mapped clients; seat v", state["input"]["seatProtocolVersion"])
PY
