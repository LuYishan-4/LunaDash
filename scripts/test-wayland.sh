#!/bin/sh
set -eu
unset MESA_GL_VERSION_OVERRIDE MESA_GLSL_VERSION_OVERRIDE
export QT_FORCE_STDERR_LOGGING=1 LUDASH_SKIP_SETUP=1
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build"}
evidence_name=wayland
host_wayland=""
if [ "${LUDASH_TEST_HOST_WAYLAND:-0}" = 1 ]; then
  case "${WAYLAND_DISPLAY:-}" in
    /*) host_wayland=$WAYLAND_DISPLAY ;;
    ?*) host_wayland=${XDG_RUNTIME_DIR:?Host XDG_RUNTIME_DIR is required}/$WAYLAND_DISPLAY ;;
    *) echo 'A running host Wayland session is required.' >&2; exit 1 ;;
  esac
  if [ ! -S "$host_wayland" ]; then echo 'Host Wayland socket is unavailable.' >&2; exit 1; fi
  evidence_name=host-wayland
fi
if [ "${LUDASH_TEST_OVERVIEW:-0}" = 1 ]; then evidence_name=desktop; fi
if [ "${LUDASH_TEST_SETUP:-0}" = 1 ]; then evidence_name=setup; export LUDASH_SKIP_SETUP=0; fi
rm -f -- "$build_dir/$evidence_name-preview.png" "$build_dir/$evidence_name-state.json"
runtime_dir=$(mktemp -d)
chmod 700 "$runtime_dir"
mkdir -p "$runtime_dir/config"
# This fixture tests normal tiled application windows, not first-use dialogs.
# A modal Files welcome intentionally overlaps its parent and would invalidate
# the non-overlap assertions below. Seed only this disposable configuration;
# tests/files/FileManagerTests.cpp exercises real first-run and chooser flows.
mkdir -p "$runtime_dir/config/LunaDash"
printf '%s\n' '{"version":1,"initialized":true,"askOnFirstOpen":true,"associations":{}}' \
  > "$runtime_dir/config/LunaDash/file-associations.json"
if [ "${LUDASH_TEST_OVERVIEW:-0}" = 1 ]; then
  mkdir -p "$runtime_dir/config/LuDash"
  printf '[desktop]\noverview=true\nshowHostDetails=false\n' > "$runtime_dir/config/LuDash/LuDash.conf"
fi
export XDG_CONFIG_HOME="$runtime_dir/config"
trap 'find "$runtime_dir" -depth -delete' EXIT
test_shell_args=""
test_demo_args="--demo"
if [ "${LUDASH_TEST_OVERVIEW:-0}" = 1 ] || [ "${LUDASH_TEST_SETUP:-0}" = 1 ]; then test_demo_args=""; fi
if [ "${LUDASH_TEST_NO_SHELL:-0}" = 1 ]; then test_shell_args="--no-shell"; fi
run_session() {
  if [ -n "$host_wayland" ]; then
    env -u LIBGL_ALWAYS_SOFTWARE XDG_RUNTIME_DIR="$runtime_dir" WAYLAND_DISPLAY="$host_wayland" QT_QPA_PLATFORM=wayland "$@"
  else
    XDG_RUNTIME_DIR="$runtime_dir" QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}" QT_XCB_GL_INTEGRATION=xcb_egl LIBGL_ALWAYS_SOFTWARE=1 \
      xvfb-run -a -s '-screen 0 1440x900x24' "$@"
  fi
}
run_session "$build_dir/ludash-compositor" \
  $test_shell_args --graphics "${LUDASH_GRAPHICS:-auto}" $test_demo_args --exit-after 8000 --screenshot "$build_dir/$evidence_name-preview.png" --state "$build_dir/$evidence_name-state.json" >"$build_dir/$evidence_name.log" 2>&1 || { cat "$build_dir/$evidence_name.log"; exit 1; }
cat "$build_dir/$evidence_name.log"
python3 "$project_dir/scripts/testing/check_graphics_log.py" "$build_dir/$evidence_name.log"
python3 - "$build_dir/$evidence_name-state.json" "$build_dir/$evidence_name-preview.png" <<'PY'
import json, sys, os
from PIL import Image
state = json.load(open(sys.argv[1]))
assert not state.get('processFailure', True), state
assert state.get('shaderReady') and not state.get('graphicsFailed'), state
assert 0 <= state['system']['cpuPercent'] <= 100, state['system']
assert 0 <= state['system']['memoryPercent'] <= 100, state['system']
assert state['system']['os'], state['system']
if os.environ.get('LUDASH_TEST_SETUP') == '1':
    assert state['setupComplete'] is False and state['layerSurfaces'] >= 3, state
if os.environ.get('LUDASH_GRAPHICS') == 'gles':
    assert state['graphicsApi'] == 'OpenGL ES', state
if os.environ.get('LUDASH_GRAPHICS') == 'opengl':
    assert state['graphicsApi'] == 'OpenGL', state
clients = [c for c in state['clients'] if not c['desktop']]
if os.environ.get("LUDASH_TEST_OVERVIEW") == "1" or os.environ.get("LUDASH_TEST_SETUP") == "1":
    assert not clients, state
    if os.environ.get('LUDASH_TEST_OVERVIEW') == '1':
        assert state['layerSurfaces'] >= 3, state
else:
    assert len(clients) >= (2 if os.environ.get("LUDASH_TEST_NO_SHELL") == "1" else 3), state
if clients and state['appearance']['blur']:
    assert state.get('blurReady') and not state.get('blurFailed'), state
assert all(c['mapped'] and c['visible'] for c in clients), state
frame = Image.open(sys.argv[2]).convert('RGB')
assert frame.getcolors(maxcolors=128) is None, 'Desktop screenshot is blank'
logical_width, logical_height = state['display']['width'], state['display']['height']
scale_x, scale_y = frame.width / logical_width, frame.height / logical_height
onscreen = []
for client in clients:
    assert client['contentWidth'] > 0 and client['contentHeight'] > 0, client
    x, y, width, height = (int(client[key]) for key in ('x', 'y', 'width', 'height'))
    assert width > 0 and height > 0 and 0 <= y < y + height <= logical_height, client
    if x < logical_width and x + width > 0:
        onscreen.append(client)
assert onscreen, 'Scrollable layout has no client intersecting the viewport'
for client in onscreen:
    x, y, width, height = (int(client[key]) for key in ('x', 'y', 'width', 'height'))
    left, right = max(0, x + 8), min(logical_width, x + width - 8)
    top, bottom = y + 40, min(logical_height, y + height - 8)
    if right - left < 16 or bottom - top < 16:
        continue
    content = frame.crop((int(left * scale_x), int(top * scale_y),
                          int(right * scale_x), int(bottom * scale_y)))
    assert content.getcolors(maxcolors=32) is None, f'Blank onscreen client content: {client}'
if os.environ.get('LUDASH_TEST_NO_SHELL') != '1':
    assert state.get('layerSurfaces', 0) >= 2, state
for i, a in enumerate(clients):
    for b in clients[i + 1:]:
        assert (a['x'] + a['width'] <= b['x'] or b['x'] + b['width'] <= a['x'] or
                a['y'] + a['height'] <= b['y'] or b['y'] + b['height'] <= a['y']), state
print('Wayland integration passed: desktop +', len(clients), 'mapped, non-overlapping clients; clean shutdown')
PY
