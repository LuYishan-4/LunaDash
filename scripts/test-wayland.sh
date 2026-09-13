#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build"}
runtime_dir=$(mktemp -d)
chmod 700 "$runtime_dir"
mkdir -p "$runtime_dir/config"
export XDG_CONFIG_HOME="$runtime_dir/config"
trap 'find "$runtime_dir" -depth -delete' EXIT
test_shell_args=""
if [ "${LUDASH_TEST_NO_SHELL:-0}" = 1 ]; then test_shell_args="--no-shell"; fi
XDG_RUNTIME_DIR="$runtime_dir" QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_egl LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a -s '-screen 0 1440x900x24' "$build_dir/ludash-compositor" \
  $test_shell_args --graphics "${LUDASH_GRAPHICS:-auto}" --demo --exit-after 8000 --screenshot "$build_dir/wayland-preview.png" --state "$build_dir/wayland-state.json" >"$build_dir/wayland.log" 2>&1 || { cat "$build_dir/wayland.log"; exit 1; }
cat "$build_dir/wayland.log"
python3 - "$build_dir/wayland-state.json" <<'PY'
import json, sys, os
state = json.load(open(sys.argv[1]))
assert not state.get('processFailure', True), state
assert state.get('shaderReady') and not state.get('graphicsFailed'), state
if os.environ.get('LUDASH_GRAPHICS') == 'gles':
    assert state['graphicsApi'] == 'OpenGL ES', state
if os.environ.get('LUDASH_GRAPHICS') == 'opengl':
    assert state['graphicsApi'] == 'OpenGL', state
clients = [c for c in state['clients'] if not c['desktop']]
assert len(clients) >= (2 if os.environ.get("LUDASH_TEST_NO_SHELL") == "1" else 3), state
assert all(c['mapped'] and c['visible'] for c in clients), state
if os.environ.get('LUDASH_TEST_NO_SHELL') != '1':
    assert state.get('layerSurfaces', 0) >= 3, state
for i, a in enumerate(clients):
    for b in clients[i + 1:]:
        assert (a['x'] + a['width'] <= b['x'] or b['x'] + b['width'] <= a['x'] or
                a['y'] + a['height'] <= b['y'] or b['y'] + b['height'] <= a['y']), state
print('Wayland integration passed: desktop +', len(clients), 'mapped, non-overlapping clients; clean shutdown')
PY
