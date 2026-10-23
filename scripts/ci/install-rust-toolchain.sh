#!/usr/bin/env bash
set -euo pipefail

version=$(rustc --version 2>/dev/null | awk '{print $2}' || true)
if [[ -n $version ]] && python3 - "$version" <<'PY'
import sys

parts = tuple(int(part) for part in sys.argv[1].split(".")[:2])
if parts >= (1, 92):
    raise SystemExit(0)
raise SystemExit(1)
PY
then
  exit 0
fi

installer=/tmp/rustup-init
rust_arch=$(uname -m)
if [[ $rust_arch != x86_64 ]]; then
    printf 'No CI rustup bootstrap target is configured for %s.\n' "$rust_arch" >&2
    exit 2
fi
rust_libc=gnu
[[ ! -e /etc/alpine-release ]] || rust_libc=musl
python3 - "$installer" "$rust_libc" <<'PY'
import pathlib
import sys
import urllib.request

destination = pathlib.Path(sys.argv[1])
libc = sys.argv[2]
urllib.request.urlretrieve(
    f"https://static.rust-lang.org/rustup/dist/x86_64-unknown-linux-{libc}/rustup-init",
    destination,
)
destination.chmod(0o755)
PY
"$installer" -y --profile minimal --default-toolchain stable
export PATH="$HOME/.cargo/bin:$PATH"
rustc --version
