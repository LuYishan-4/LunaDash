#!/bin/sh
# Run the full UI/frontend integration suite on an isolated display and bus.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=${1:?usage: run_runtime.sh BUILD_DIR [PYTHON]}
python=${2:-python3}
runtime=$(mktemp -d)
trap 'rm -rf "$runtime"' EXIT
chmod 700 "$runtime"
XDG_RUNTIME_DIR="$runtime" xvfb-run -a -s '-screen 0 1440x1000x24' \
    dbus-run-session -- "$python" "$root/tests/portal/test_runtime.py" "$build"
