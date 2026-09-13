#!/bin/sh
set -eu
unset MESA_GL_VERSION_OVERRIDE MESA_GLSL_VERSION_OVERRIDE
export QT_FORCE_STDERR_LOGGING=1
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build"}
evidence_name=wayland
if [ "${LUDASH_TEST_OVERVIEW:-0}" = 1 ]; then evidence_name=desktop; fi
rm -f -- "$build_dir/$evidence_name-preview.png" "$build_dir/$evidence_name-state.json"
runtime_dir=$(mktemp -d)
chmod 700 "$runtime_dir"
mkdir -p "$runtime_dir/config"
export XDG_CONFIG_HOME="$runtime_dir/config"
trap 'find "$runtime_dir" -depth -delete' EXIT
test_shell_args=""
test_demo_args="--demo"
if [ "${LUDASH_TEST_OVERVIEW:-0}" = 1 ]; then test_demo_args=""; fi
if [ "${LUDASH_TEST_NO_SHELL:-0}" = 1 ]; then test_shell_args="--no-shell"; fi
XDG_RUNTIME_DIR="$runtime_dir" QT_QPA_PLATFORM=xcb QT_XCB_GL_INTEGRATION=xcb_egl LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a -s '-screen 0 1440x900x24' "$build_dir/ludash-compositor" \
  $test_shell_args --graphics "${LUDASH_GRAPHICS:-auto}" $test_demo_args --exit-after 8000 --screenshot "$build_dir/$evidence_name-preview.png" --state "$build_dir/$evidence_name-state.json" >"$build_dir/$evidence_name.log" 2>&1 || { cat "$build_dir/$evidence_name.log"; exit 1; }
cat "$build_dir/$evidence_name.log"
python3 - "$build_dir/$evidence_name-state.json" "$build_dir/$evidence_name-preview.png" <<'PY'
import json, sys, os
from PIL import Image
state = json.load(open(sys.argv[1]))
assert not state.get('processFailure', True), state
assert state.get('shaderReady') and not state.get('graphicsFailed'), state
assert 0 <= state['system']['cpuPercent'] <= 100, state['system']
assert 0 <= state['system']['memoryPercent'] <= 100, state['system']
assert state['system']['os'], state['system']
if os.environ.get('LUDASH_GRAPHICS') == 'gles':
    assert state['graphicsApi'] == 'OpenGL ES', state
if os.environ.get('LUDASH_GRAPHICS') == 'opengl':
    assert state['graphicsApi'] == 'OpenGL', state
clients = [c for c in state['clients'] if not c['desktop']]
if os.environ.get("LUDASH_TEST_OVERVIEW") == "1":
    assert not clients, state
else:
    assert len(clients) >= (2 if os.environ.get("LUDASH_TEST_NO_SHELL") == "1" else 3), state
assert all(c['mapped'] and c['visible'] for c in clients), state
frame = Image.open(sys.argv[2]).convert('RGB')
assert frame.getcolors(maxcolors=128) is None, 'Desktop screenshot is blank'
for client in clients:
    assert client['contentWidth'] > 0 and client['contentHeight'] > 0, client
    x, y, width, height = (int(client[key]) for key in ('x', 'y', 'width', 'height'))
    assert 0 <= x < x + width <= frame.width and 0 <= y < y + height <= frame.height, client
    content = frame.crop((x + 8, y + 40, x + width - 8, y + height - 8))
    assert content.getcolors(maxcolors=32) is None, f'Blank client content: {client}'
if os.environ.get('LUDASH_TEST_NO_SHELL') != '1':
    assert state.get('layerSurfaces', 0) >= 3, state
for i, a in enumerate(clients):
    for b in clients[i + 1:]:
        assert (a['x'] + a['width'] <= b['x'] or b['x'] + b['width'] <= a['x'] or
                a['y'] + a['height'] <= b['y'] or b['y'] + b['height'] <= a['y']), state
print('Wayland integration passed: desktop +', len(clients), 'mapped, non-overlapping clients; clean shutdown')
PY
