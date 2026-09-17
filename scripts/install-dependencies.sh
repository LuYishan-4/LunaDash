#!/usr/bin/env bash
# Install LunaDash build/runtime dependencies from the current distribution.
set -euo pipefail

dry_run=false
usage() {
    cat <<'EOF'
Usage: ./scripts/install-dependencies.sh [--dry-run]

Supported package managers:
  pacman   Arch Linux and derivatives
  apt-get  Debian / Ubuntu and derivatives
  dnf      Fedora and derivatives
  zypper   openSUSE Tumbleweed / Slowroll

The script installs only packages from enabled distribution repositories. It does
not add third-party repositories. Quickshell is checked separately because its
packaging differs between distributions.
EOF
}
while (($#)); do
    case "$1" in
        --dry-run) dry_run=true ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if ((EUID == 0)); then
    if [[ ${LUDASH_ALLOW_ROOT_DEPS:-0} != 1 ]]; then
        echo 'Run as a normal user; privilege elevation is requested only for package installation.' >&2
        exit 1
    fi
    elevate=()
elif command -v sudo >/dev/null 2>&1; then
    elevate=(sudo)
elif command -v doas >/dev/null 2>&1; then
    elevate=(doas)
else
    echo 'sudo or doas is required to install system packages.' >&2
    exit 1
fi

run() {
    printf '+ '; printf '%q ' "$@"; printf '\n'
    if ! $dry_run; then "$@"; fi
}

manager=''
if command -v pacman >/dev/null 2>&1; then manager=pacman
elif command -v apt-get >/dev/null 2>&1; then manager=apt
elif command -v dnf >/dev/null 2>&1; then manager=dnf
elif command -v zypper >/dev/null 2>&1; then manager=zypper
else
    cat >&2 <<'EOF'
No supported package manager was found.
Install a C++20 compiler, CMake >= 3.21, Ninja, pkg-config, Qt 6.4+ Base/
Declarative/Wayland/OpenGL development packages, Wayland development headers,
libinput, libxkbcommon, udev/systemd development headers, GL development headers,
glib2, shared-mime-info, Fish, and Quickshell 0.3+; then rerun the installer.
EOF
    exit 1
fi

case "$manager" in
    pacman)
        run "${elevate[@]}" pacman -S --needed \
            base-devel cmake ninja git pkgconf \
            libglvnd mesa wayland wayland-protocols libinput libxkbcommon \
            systemd glib2 qt6-base qt6-declarative qt6-wayland qt6-translations \
            shared-mime-info fish
        if ! command -v quickshell >/dev/null 2>&1 && pacman -Si quickshell >/dev/null 2>&1; then
            run "${elevate[@]}" pacman -S --needed quickshell
        fi
        ;;
    apt)
        run "${elevate[@]}" apt-get update
        run "${elevate[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y \
            build-essential cmake ninja-build git pkg-config \
            libgl-dev libwayland-dev wayland-protocols libinput-dev \
            libxkbcommon-dev libudev-dev libglib2.0-dev \
            qt6-base-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland \
            libqt6opengl6-dev shared-mime-info fish
        ;;
    dnf)
        run "${elevate[@]}" dnf install -y \
            gcc gcc-c++ cmake ninja-build git pkgconf-pkg-config \
            mesa-libGL-devel wayland-devel wayland-protocols-devel libinput-devel \
            libxkbcommon-devel systemd-devel glib2-devel \
            qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland-devel \
            shared-mime-info fish
        ;;
    zypper)
        run "${elevate[@]}" zypper --non-interactive install \
            gcc gcc-c++ cmake ninja git pkg-config \
            Mesa-libGL-devel wayland-devel wayland-protocols-devel libinput-devel \
            libxkbcommon-devel systemd-devel glib2-devel \
            libqt6-qtbase-devel libqt6-qtdeclarative-devel libqt6-qtwayland-devel \
            shared-mime-info fish
        ;;
esac

if $dry_run; then
    echo "Dry run complete for package manager: $manager"
    exit 0
fi

if ! command -v quickshell >/dev/null 2>&1; then
    cat <<'EOF'

Build dependencies are installed, but Quickshell was not found.
Install Quickshell 0.3 or newer using your distribution package if available, or
follow the upstream installation guide:
  https://quickshell.org/docs/v0.3.0/guide/install-setup/

LunaDash can still be compiled now; the shell requires Quickshell at runtime.
EOF
else
    printf 'Dependencies ready (%s); Quickshell: %s\n' "$manager" "$(command -v quickshell)"
fi
