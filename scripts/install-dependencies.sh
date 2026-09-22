#!/usr/bin/env bash
# Install LunaDash build/runtime dependencies from the current distribution.
set -euo pipefail

dry_run=false
usage() {
    cat <<'EOF'
Usage: ./scripts/install-dependencies.sh [--dry-run]

Supported package managers:
  pacman        Arch Linux and derivatives
  apt-get       Debian / Ubuntu and derivatives
  dnf           Fedora / RHEL-family derivatives
  zypper        openSUSE Tumbleweed / Slowroll / Leap
  apk           Alpine Linux
  xbps-install  Void Linux
  emerge        Gentoo Linux

If none of these package managers exists, the script enters generic validation
mode and accepts an already-provisioned CMake/Ninja/Qt6/Wayland toolchain.

The script installs only packages from enabled distribution repositories. It does
not add third-party repositories. Quickshell is checked separately because its
packaging differs between distributions. Fcitx 5, its Qt integration, and its
configuration tool are installed automatically on supported distributions.
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
    elevate=()
fi

run() {
    printf '+ '; printf '%q ' "$@"; printf '\n'
    if ! $dry_run; then "$@"; fi
}

manager='generic'
if command -v pacman >/dev/null 2>&1; then manager=pacman
elif command -v apt-get >/dev/null 2>&1; then manager=apt
elif command -v dnf >/dev/null 2>&1; then manager=dnf
elif command -v zypper >/dev/null 2>&1; then manager=zypper
elif command -v apk >/dev/null 2>&1; then manager=apk
elif command -v xbps-install >/dev/null 2>&1; then manager=xbps
elif command -v emerge >/dev/null 2>&1; then manager=emerge
fi

if [[ $manager != generic && ${#elevate[@]} -eq 0 && $EUID -ne 0 ]]; then
    echo 'sudo or doas is required to install system packages.' >&2
    exit 1
fi

case "$manager" in
    pacman)
        pacman_install=(pacman -S --needed)
        if [[ ${LUDASH_ALLOW_ROOT_DEPS:-0} == 1 ]]; then
            pacman_install+=(--noconfirm)
        fi
        run "${elevate[@]}" "${pacman_install[@]}" \
            base-devel cmake ninja git python pkgconf \
            libglvnd mesa wayland wayland-protocols libinput libxkbcommon \
            systemd glib2 qt6-base qt6-declarative qt6-wayland qt6-translations \
            shared-mime-info kitty fish grim slurp wl-clipboard brightnessctl ddcutil polkit-kde-agent \
            xdg-desktop-portal xdg-desktop-portal-gtk xdg-desktop-portal-wlr pipewire \
            fcitx5 fcitx5-qt fcitx5-configtool networkmanager network-manager-applet \
            wireplumber pavucontrol bluez bluez-utils blueman system-config-printer \
            gnome-disk-utility partitionmanager firewalld seahorse pciutils usbutils \
            udisks2 power-profiles-daemon gnome-control-center
        wlroots_package=wlroots0.20
        if ! pacman -Si "$wlroots_package" >/dev/null 2>&1; then
            wlroots_package=wlroots
        fi
        run "${elevate[@]}" "${pacman_install[@]}" "$wlroots_package"
        if ! command -v quickshell >/dev/null 2>&1 && pacman -Si quickshell >/dev/null 2>&1; then
            run "${elevate[@]}" "${pacman_install[@]}" quickshell
        fi
        ;;
    apt)
        run "${elevate[@]}" apt-get update
        wlroots_package=''
        for candidate in libwlroots-0.20-dev libwlroots-0.19-dev libwlroots-0.18-dev libwlroots-dev; do
            if apt-cache show "$candidate" >/dev/null 2>&1; then
                wlroots_package=$candidate
                break
            fi
        done
        if [[ -z $wlroots_package ]]; then
            echo 'No supported wlroots development package (>= 0.17) is available.' >&2
            exit 1
        fi
        run "${elevate[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y \
            build-essential cmake ninja-build git python3 pkg-config \
            libgl-dev libwayland-dev wayland-protocols libinput-dev \
            libxkbcommon-dev libudev-dev libglib2.0-dev \
            "$wlroots_package" qt6-base-dev qt6-declarative-dev qt6-wayland \
            libqt6opengl6-dev shared-mime-info kitty fish grim slurp wl-clipboard brightnessctl ddcutil \
            xdg-desktop-portal xdg-desktop-portal-gtk xdg-desktop-portal-wlr pipewire \
            fcitx5 fcitx5-frontend-qt6 fcitx5-config-qt
        ;;
    dnf)
        run "${elevate[@]}" dnf install -y \
            gcc gcc-c++ cmake ninja-build git python3 pkgconf-pkg-config \
            mesa-libGL-devel wayland-devel wayland-protocols-devel libinput-devel \
            libxkbcommon-devel systemd-devel glib2-devel \
            wlroots-devel qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtwayland \
            shared-mime-info kitty fish grim slurp wl-clipboard brightnessctl ddcutil \
            xdg-desktop-portal xdg-desktop-portal-gtk xdg-desktop-portal-wlr pipewire \
            fcitx5 fcitx5-qt fcitx5-qt6 fcitx5-configtool
        ;;
    zypper)
        run "${elevate[@]}" zypper --non-interactive install \
            gcc gcc-c++ cmake ninja git python3 pkg-config \
            Mesa-libGL-devel wayland-devel wayland-protocols-devel libinput-devel \
            libxkbcommon-devel systemd-devel glib2-devel \
            wlroots-devel qt6-base-devel qt6-declarative-devel qt6-wayland \
            shared-mime-info kitty fish grim slurp wl-clipboard brightnessctl ddcutil \
            xdg-desktop-portal xdg-desktop-portal-gtk xdg-desktop-portal-wlr pipewire \
            fcitx5 fcitx5-qt6 fcitx5-configtool
        ;;
    apk)
        run "${elevate[@]}" apk add \
            build-base cmake ninja git python3 pkgconf mesa-dev \
            wayland-dev wayland-protocols libinput-dev libxkbcommon-dev eudev-dev \
            glib-dev wlroots-dev qt6-qtbase-dev qt6-qtdeclarative-dev qt6-qtwayland \
            shared-mime-info kitty fish grim slurp wl-clipboard brightnessctl ddcutil \
            xdg-desktop-portal xdg-desktop-portal-gtk xdg-desktop-portal-wlr pipewire \
            fcitx5 fcitx5-qt fcitx5-configtool
        ;;
    xbps)
        run "${elevate[@]}" xbps-install -Sy \
            base-devel cmake ninja git python3 pkg-config MesaLib-devel \
            wayland-devel wayland-protocols libinput-devel libxkbcommon-devel \
            eudev-libudev-devel glib-devel wlroots-devel qt6-base-devel qt6-declarative-devel \
            qt6-wayland shared-mime-info kitty fish grim slurp wl-clipboard brightnessctl ddcutil \
            xdg-desktop-portal xdg-desktop-portal-gtk xdg-desktop-portal-wlr pipewire \
            fcitx5 fcitx5-qt fcitx5-configtool
        ;;
    emerge)
        run "${elevate[@]}" emerge --noreplace \
            dev-build/cmake app-alternatives/ninja virtual/pkgconfig dev-vcs/git dev-lang/python \
            media-libs/mesa dev-libs/wayland dev-libs/wayland-protocols \
            dev-libs/libinput x11-libs/libxkbcommon virtual/udev dev-libs/glib \
            gui-libs/wlroots dev-qt/qtbase:6 dev-qt/qtdeclarative:6 dev-qt/qtwayland:6 \
            sys-apps/xdg-desktop-portal sys-apps/xdg-desktop-portal-gtk gui-libs/xdg-desktop-portal-wlr media-video/pipewire gui-apps/grim gui-apps/slurp gui-apps/wl-clipboard app-misc/brightnessctl app-misc/ddcutil x11-misc/shared-mime-info app-shells/fish x11-terms/kitty \
            app-i18n/fcitx app-i18n/fcitx-qt app-i18n/fcitx-configtool
        ;;
    generic)
        missing=()
        for tool in cmake ninja pkg-config git python3 c++; do
            command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
        done
        if ((${#missing[@]})); then
            printf 'Generic Linux mode: missing required build tools: %s\n' "${missing[*]}" >&2
            cat >&2 <<'EOF'
Install a C++20 compiler, CMake >= 3.21, Ninja, Python 3, pkg-config, wlroots >= 0.17,
Qt 6.4+ Base/Declarative/OpenGL packages, the Qt Wayland client plugin,
Wayland development headers, libinput, libxkbcommon, udev development headers,
GL development headers, GLib,
shared-mime-info and Fish; then rerun the installer.
EOF
            exit 1
        fi
        if ! command -v fcitx5 >/dev/null 2>&1; then
            cat >&2 <<'EOF'
Generic Linux mode: Fcitx 5 was not found.
Install Fcitx 5, its Qt 6 input method module, and fcitx5-configtool using your
package manager before starting a LunaDash session.
EOF
        fi
        echo 'Generic Linux mode: package installation skipped; existing toolchain will be validated by CMake.'
        ;;
esac

if $dry_run; then
    echo "Dry run complete for package manager: $manager"
    exit 0
fi

if ! command -v fcitx5 >/dev/null 2>&1; then
    echo 'Warning: Fcitx 5 is still not in PATH after dependency installation.' >&2
else
    printf 'Fcitx 5 ready: %s\n' "$(command -v fcitx5)"
fi

if ! command -v quickshell >/dev/null 2>&1; then
    cat <<'EOF'

Build dependencies are ready, but Quickshell was not found.
Install Quickshell 0.3 or newer using your distribution package if available, or
follow the upstream installation guide:
  https://quickshell.org/docs/v0.3.0/guide/install-setup/

LunaDash can still be compiled now; the shell requires Quickshell at runtime.
EOF
else
    printf 'Dependencies ready (%s); Quickshell: %s\n' "$manager" "$(command -v quickshell)"
fi
