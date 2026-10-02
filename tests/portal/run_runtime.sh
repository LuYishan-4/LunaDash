#!/bin/sh
# Run the full UI/frontend integration suite on an isolated display and bus.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build=${1:?usage: run_runtime.sh BUILD_DIR [PYTHON]}
python=${2:-python3}
runtime=$(mktemp -d)
cleanup() {
    # The D-Bus activated document portal unmounts FUSE asynchronously when
    # dbus-run-session exits. Do not traverse its mount during shutdown.
    attempt=0
    while mountpoint -q "$runtime/doc"; do
        if [ "$attempt" -ge 50 ]; then
            echo "Document portal did not unmount $runtime/doc" >&2
            return 1
        fi
        sleep 0.1
        attempt=$((attempt + 1))
    done
    rm -rf "$runtime"
}
trap cleanup EXIT
chmod 700 "$runtime"
XDG_RUNTIME_DIR="$runtime" xvfb-run -a -s '-screen 0 1440x1000x24' \
    dbus-run-session -- "$python" "$root/tests/portal/test_runtime.py" "$build"
