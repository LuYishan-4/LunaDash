#!/usr/bin/env bash
set -euo pipefail

output_dir=${1:?usage: test-xdpw-sources.sh OUTPUT_DIR}
mkdir -p -- "$output_dir"

xdpw=${XDPW_BINARY:-}
if [[ -z "$xdpw" ]]; then
    xdpw=$(find /usr -type f -name xdg-desktop-portal-wlr -perm -111 -print -quit 2>/dev/null || true)
fi
if [[ -z "$xdpw" || ! -x "$xdpw" ]]; then
    echo "xdg-desktop-portal-wlr executable not found" >&2
    exit 2
fi
command -v pipewire >/dev/null || { echo "pipewire executable not found" >&2; exit 2; }
command -v gdbus >/dev/null || { echo "gdbus executable not found" >&2; exit 2; }

pipewire >"$output_dir/pipewire.log" 2>&1 &
pw_pid=$!
xdpw_pid=""

cleanup() {
    if [[ -n "$xdpw_pid" ]]; then
        kill "$xdpw_pid" 2>/dev/null || true
        wait "$xdpw_pid" 2>/dev/null || true
    fi
    kill "$pw_pid" 2>/dev/null || true
    wait "$pw_pid" 2>/dev/null || true
}
trap cleanup EXIT

for _ in $(seq 1 50); do
    [[ -S "$XDG_RUNTIME_DIR/pipewire-0" ]] && break
    kill -0 "$pw_pid"
    sleep 0.1
done
[[ -S "$XDG_RUNTIME_DIR/pipewire-0" ]]

xdpw_args=(-l DEBUG)
if [[ -n "${XDPW_CONFIG:-}" ]]; then
    [[ -r "$XDPW_CONFIG" ]] || {
        echo "Requested xdpw config is not readable: $XDPW_CONFIG" >&2
        exit 2
    }
    xdpw_args+=(-c "$XDPW_CONFIG")
fi
"$xdpw" "${xdpw_args[@]}" >"$output_dir/xdpw.log" 2>&1 &
xdpw_pid=$!

types=""
for _ in $(seq 1 80); do
    if types=$(gdbus call --session \
        --dest org.freedesktop.impl.portal.desktop.wlr \
        --object-path /org/freedesktop/portal/desktop \
        --method org.freedesktop.DBus.Properties.Get \
        org.freedesktop.impl.portal.ScreenCast AvailableSourceTypes 2>/dev/null); then
        break
    fi
    kill -0 "$xdpw_pid"
    sleep 0.1
done

if [[ -n "${XDPW_CONFIG:-}" ]]; then
    grep -Fq "chooser_type: dmenu" "$output_dir/xdpw.log"
    grep -Fq "xdg-desktop-portal-lunadash --screencast-chooser" "$output_dir/xdpw.log"
fi

printf "%s\n" "$types" | tee "$output_dir/portal-source-types.txt"
value=$(printf "%s\n" "$types" | sed -E "s/.*uint32 ([0-9]+).*/\\1/")
if [[ ! "$value" =~ ^[0-9]+$ ]]; then
    echo "Could not parse ScreenCast AvailableSourceTypes: $types" >&2
    exit 1
fi
if (( (value & 1) == 0 )); then
    echo "ScreenCast backend does not advertise MONITOR capture: $value" >&2
    exit 1
fi
if (( (value & 2) == 0 )); then
    echo "ScreenCast backend does not advertise WINDOW capture: $value" >&2
    exit 1
fi

echo "ScreenCast portal source types verified: $value (MONITOR + WINDOW)"
