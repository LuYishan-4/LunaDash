#!/usr/bin/env bash
# Build/install LunaDash and optionally configure SDDM login.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
enable_sddm=false
autologin=""
dry_run=false
skip_deps=false
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build-install"}

usage() {
    cat <<'EOF'
Usage: ./scripts/install-session.sh [--enable-sddm] [--autologin USER] [--skip-deps] [--dry-run]

Supported installation paths:
  Arch Linux / derivatives      makepkg + pacman package installation
  Debian / Ubuntu derivatives   standard CMake system installation
  Fedora derivatives            standard CMake system installation
  openSUSE Tumbleweed/Slowroll  standard CMake system installation

Options:
  --enable-sddm   Install/enable SDDM and graphical boot; never restart a desktop.
  --autologin USER
                  Opt into passwordless SDDM login for this user on boot.
                  Requires --enable-sddm. Existing auto-login config is preserved.
  --skip-deps     Do not invoke scripts/install-dependencies.sh.
  --dry-run       Print commands without building or modifying the system.

Run as your normal user. Privilege elevation is requested only for package/system
installation. LunaDash is a development preview; physical GPU/seat/VT support is
still less tested than nested sessions.
EOF
}

while (($#)); do
    case "$1" in
        --enable-sddm) enable_sddm=true ;;
        --autologin)
            (($# >= 2)) && [[ -n $2 ]] || { usage >&2; exit 2; }
            autologin=$2
            shift
            ;;
        --skip-deps) skip_deps=true ;;
        --dry-run) dry_run=true ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if ((EUID == 0)); then
    echo 'Run as a normal user; build/package tools must not run as root.' >&2
    exit 1
fi

if command -v sudo >/dev/null 2>&1; then
    elevate=(sudo)
elif command -v doas >/dev/null 2>&1; then
    elevate=(doas)
else
    echo 'sudo or doas is required for system installation.' >&2
    exit 1
fi

package_manager='generic'
if command -v pacman >/dev/null 2>&1; then package_manager='pacman'
elif command -v apt-get >/dev/null 2>&1; then package_manager='apt'
elif command -v dnf >/dev/null 2>&1; then package_manager='dnf'
elif command -v zypper >/dev/null 2>&1; then package_manager='zypper'
fi

if [[ -n $autologin ]]; then
    if ! $enable_sddm; then
        echo '--autologin requires --enable-sddm.' >&2
        exit 2
    fi
    if [[ ! $autologin =~ ^[a-z_][a-z0-9_-]*[$]?$ ]] ||
       ! id "$autologin" >/dev/null 2>&1 || [[ $(id -u "$autologin") == 0 ]]; then
        echo 'Auto-login requires an existing, non-root local account name.' >&2
        exit 2
    fi
    if [[ -e /etc/sddm.conf.d/90-ludash-autologin.conf || -L /etc/sddm.conf.d/90-ludash-autologin.conf ]]; then
        echo 'Auto-login configuration already exists. Review it manually before reinstalling with this option.' >&2
        exit 1
    fi
fi

if $enable_sddm; then
    command -v systemctl >/dev/null 2>&1 || { echo 'SDDM setup requires systemd.' >&2; exit 1; }
    manager=$(readlink -f /etc/systemd/system/display-manager.service || true)
    if [[ -n $manager && -e $manager && ${manager##*/} != sddm.service ]]; then
        echo 'Another display manager is enabled. Install LunaDash without --enable-sddm and select it there.' >&2
        exit 1
    fi
fi

run() {
    printf '+ '; printf '%q ' "$@"; printf '\n'
    if ! $dry_run; then "$@"; fi
}

if ! $skip_deps; then
    deps=(bash "$project_dir/scripts/install-dependencies.sh")
    $dry_run && deps+=(--dry-run)
    run "${deps[@]}"
fi

if [[ $package_manager == pacman ]]; then
    command -v makepkg >/dev/null 2>&1 || { echo 'makepkg is required on Arch Linux.' >&2; exit 1; }
    run "$project_dir/scripts/make-source.sh"
    (
        cd -- "$project_dir/packaging/arch"
        run makepkg --syncdeps --force --install
    )
else
    for tool in cmake ninja; do
        command -v "$tool" >/dev/null 2>&1 || { echo "$tool is required. Run bash scripts/install-dependencies.sh first." >&2; exit 1; }
    done
    run cmake -S "$project_dir" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    run cmake --build "$build_dir" --parallel
    run "${elevate[@]}" cmake --install "$build_dir"
fi

install_sddm() {
    case "$package_manager" in
        pacman) run "${elevate[@]}" pacman -S --needed sddm ;;
        apt) run "${elevate[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y sddm ;;
        dnf) run "${elevate[@]}" dnf install -y sddm ;;
        zypper) run "${elevate[@]}" zypper --non-interactive install sddm ;;
        *)
            command -v sddm >/dev/null 2>&1 || {
                echo 'Install SDDM with your distribution package manager, then rerun --enable-sddm.' >&2
                exit 1
            }
            ;;
    esac
}

if $enable_sddm; then
    install_sddm
    run "${elevate[@]}" systemctl enable sddm.service
    run "${elevate[@]}" systemctl set-default graphical.target
fi

if [[ -n $autologin ]]; then
    if $dry_run; then
        printf 'Would create /etc/sddm.conf.d/90-ludash-autologin.conf for %s, session ludash.desktop (Relogin=false).\n' "$autologin"
    else
        config=$(mktemp)
        trap 'rm -f -- "$config"' EXIT
        printf '[Autologin]\nUser=%s\nSession=ludash.desktop\nRelogin=false\n' "$autologin" > "$config"
        run "${elevate[@]}" install -d -m 755 /etc/sddm.conf.d
        run "${elevate[@]}" sh -c 'set -C; umask 022; cat "$1" > /etc/sddm.conf.d/90-ludash-autologin.conf' sh "$config"
    fi
fi

if $dry_run; then
    echo "Dry run complete for installation path: $package_manager"
    exit 0
fi

if ! command -v quickshell >/dev/null 2>&1; then
    cat <<'EOF'
LunaDash was installed, but Quickshell is not in PATH.
Install Quickshell 0.3 or newer before starting a LunaDash desktop session:
  https://quickshell.org/docs/v0.3.0/guide/install-setup/
EOF
fi

cat <<EOF
Finished using installation path: $package_manager
Save your work, then log out or reboot when ready.
Choose LunaDash (Wayland) in the login screen. The first-run guide opens on login.
Recovery: select your previous desktop; remove only the optional LunaDash
auto-login file from a TTY if needed. See docs/LOGIN_SESSION.md.
EOF
