#!/usr/bin/env bash
# Build/install LunaDash and optionally configure SDDM login.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
enable_sddm=false
autologin=""
dry_run=false
skip_deps=false
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build-install"}
progress_file=${LUDASH_INSTALL_PROGRESS_FILE:-}

report_progress() {
    local progress=$1 stage=$2 message=$3
    [[ -n $progress_file ]] || return 0
    local tmp="${progress_file}.tmp"
    printf '%s|%s|%s\n' "$progress" "$stage" "$message" > "$tmp"
    mv -f "$tmp" "$progress_file"
}

usage() {
    cat <<'EOF'
Usage: ./scripts/install-session.sh [--enable-sddm] [--autologin USER] [--skip-deps] [--dry-run]

Supported installation paths:
  Arch Linux / derivatives      makepkg + pacman package installation
  Debian / Ubuntu derivatives   standard CMake system installation
  Fedora derivatives            standard CMake system installation
  openSUSE family               standard CMake system installation
  Alpine Linux                  apk dependencies + standard CMake install
  Void Linux                    xbps dependencies + standard CMake install
  Gentoo Linux                  emerge dependencies + standard CMake install
  Other Linux distributions     existing toolchain + standard CMake install

Options:
  --enable-sddm   Install/enable SDDM on systemd systems; never restart a desktop.
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

if [[ ${LUDASH_PREFER_PKEXEC:-0} == 1 ]]; then
    command -v pkexec >/dev/null 2>&1 || {
        echo 'pkexec is required for graphical LunaDash updates.' >&2
        exit 1
    }
    elevate=(pkexec)
elif command -v sudo >/dev/null 2>&1; then
    elevate=(sudo)
elif command -v doas >/dev/null 2>&1; then
    elevate=(doas)
else
    echo 'sudo, doas, or graphical pkexec elevation is required for system installation.' >&2
    exit 1
fi

package_manager='generic'
if command -v pacman >/dev/null 2>&1; then package_manager='pacman'
elif command -v apt-get >/dev/null 2>&1; then package_manager='apt'
elif command -v dnf >/dev/null 2>&1; then package_manager='dnf'
elif command -v zypper >/dev/null 2>&1; then package_manager='zypper'
elif command -v apk >/dev/null 2>&1; then package_manager='apk'
elif command -v xbps-install >/dev/null 2>&1; then package_manager='xbps'
elif command -v emerge >/dev/null 2>&1; then package_manager='emerge'
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
    command -v systemctl >/dev/null 2>&1 || {
        echo '--enable-sddm currently supports systemd systems only. Install/enable your display manager manually on OpenRC/runit systems.' >&2
        exit 1
    }
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

report_progress 36 prepare "Preparing the LunaDash installation script."

if ! $skip_deps; then
    report_progress 38 dependencies "Installing or verifying build dependencies."
    deps=(bash "$project_dir/scripts/install-dependencies.sh")
    $dry_run && deps+=(--dry-run)
    run "${deps[@]}"
fi

if [[ $package_manager == pacman ]]; then
    command -v makepkg >/dev/null 2>&1 || { echo 'makepkg is required on Arch Linux.' >&2; exit 1; }
    report_progress 42 package "Preparing the Arch Linux package source."
    run "$project_dir/scripts/make-source.sh"
    (
        cd -- "$project_dir/packaging/arch"
        if [[ ${LUDASH_PREFER_PKEXEC:-0} == 1 ]]; then
            report_progress 54 build "Building the Arch Linux package."
            run makepkg --force --cleanbuild
            mapfile -t packages < <(makepkg --packagelist)
            (("${#packages[@]}" > 0)) || { echo 'makepkg produced no package paths.' >&2; exit 1; }
            for package in "${packages[@]}"; do
                [[ -f $package ]] || { echo "Built package is missing: $package" >&2; exit 1; }
            done
            report_progress 84 install "Installing the newly built LunaDash package."
            run "${elevate[@]}" pacman -U --noconfirm "${packages[@]}"
        else
            report_progress 54 build "Building and installing the Arch Linux package."
            run makepkg --syncdeps --force --install
        fi
    )
else
    for tool in cmake ninja; do
        command -v "$tool" >/dev/null 2>&1 || { echo "$tool is required. Run bash scripts/install-dependencies.sh first." >&2; exit 1; }
    done
    report_progress 44 configure "Configuring the LunaDash release build."
    run cmake -S "$project_dir" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    report_progress 58 build "Building LunaDash."
    run cmake --build "$build_dir" --parallel
    report_progress 84 install "Installing LunaDash system files."
    run "${elevate[@]}" cmake --install "$build_dir"
fi

report_progress 96 finalize "Finalizing the LunaDash installation."

install_sddm() {
    case "$package_manager" in
        pacman) run "${elevate[@]}" pacman -S --needed sddm ;;
        apt) run "${elevate[@]}" env DEBIAN_FRONTEND=noninteractive apt-get install -y sddm ;;
        dnf) run "${elevate[@]}" dnf install -y sddm ;;
        zypper) run "${elevate[@]}" zypper --non-interactive install sddm ;;
        apk) run "${elevate[@]}" apk add sddm ;;
        xbps) run "${elevate[@]}" xbps-install -Sy sddm ;;
        emerge) run "${elevate[@]}" emerge --noreplace x11-misc/sddm ;;
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
