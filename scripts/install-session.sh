#!/usr/bin/env bash
# Build/install LunaDash and optionally configure SDDM login.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
enable_sddm=false
autologin=""
dry_run=false
skip_deps=false
non_interactive=false
[[ ${LUDASH_UPDATE_MODE:-0} == 1 ]] && non_interactive=true
build_dir=${LUDASH_BUILD_DIR:-"$project_dir/build-install"}
progress_file=${LUDASH_INSTALL_PROGRESS_FILE:-}
guide_mode=auto
setup_apps=''
setup_author=false
setup_desktop=false
setup_requested=false
# Keep first-install choices separate from the established update/build path.
source "$project_dir/scripts/setup-guide.sh"

report_progress() {
    local progress=$1 stage=$2 message=$3
    [[ -n $progress_file ]] || return 0
    local tmp="${progress_file}.tmp"
    printf '%s|%s|%s\n' "$progress" "$stage" "$message" > "$tmp"
    mv -f "$tmp" "$progress_file"
}

usage() {
    cat <<'EOF'
Usage: ./scripts/install-session.sh [--guided|--skip-guide] [--apps GROUPS]
       [--author-config] [--desktop-profile] [--enable-sddm] [--autologin USER]
       [--skip-deps] [--non-interactive] [--dry-run]

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
  --guided       Show the first-install guide again (requires a terminal).
                  Opens automatically for a new interactive installation.
  --skip-guide   Keep the direct core installation path without questions.
  --apps GROUPS  Optional Arch packages; comma-separated basics, desktop, media,
                  office, development, input, or none. See data/setup/README.md.
  --author-config
                  Seed editable, portable Kitty/Fish/Starship/Fastfetch templates
                  inspired by the author's settings. Existing files are preserved.
  --desktop-profile
                  Seed the LunaDash top-panel profile only if no profile exists.
  --enable-sddm   Install/enable SDDM on systemd systems; never restart a desktop.
  --autologin USER
                  Opt into passwordless SDDM login for this user on boot.
                  Requires --enable-sddm. Existing auto-login config is preserved.
  --skip-deps     Do not invoke scripts/install-dependencies.sh.
  --non-interactive
                  Accept package confirmation prompts and never read terminal input.
                  Requires --skip-deps; cannot configure SDDM/autologin.
                  Administrator authorization is still required.
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
        --guided) guide_mode=always ;;
        --skip-guide) guide_mode=never ;;
        --apps)
            (($# >= 2)) && [[ -n $2 ]] || { usage >&2; exit 2; }
            setup_apps=$2
            setup_requested=true
            shift
            ;;
        --author-config) setup_author=true; setup_requested=true ;;
        --desktop-profile) setup_desktop=true; setup_requested=true ;;
        --non-interactive) non_interactive=true ;;
        --dry-run) dry_run=true ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

if $non_interactive && { ! $skip_deps || $enable_sddm || [[ -n $autologin ]]; }; then
    echo 'Non-interactive updates require --skip-deps and cannot configure the display manager.' >&2
    exit 2
fi
if $non_interactive && { [[ $guide_mode == always ]] || $setup_requested; }; then
    echo 'First-install choices are unavailable during non-interactive updates.' >&2
    exit 2
fi

if ((EUID == 0)); then
    echo 'Run as a normal user; build/package tools must not run as root.' >&2
    exit 1
fi

if [[ ${LUDASH_PREFER_PKEXEC:-0} == 1 ]]; then
    command -v pkexec >/dev/null 2>&1 || {
        echo 'pkexec is required for graphical LunaDash updates.' >&2
        exit 1
    }
    elevate=(pkexec --disable-internal-agent)
elif command -v sudo >/dev/null 2>&1; then
    elevate=(sudo)
    $non_interactive && elevate+=(-n)
elif command -v doas >/dev/null 2>&1; then
    elevate=(doas)
    $non_interactive && elevate+=(-n)
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

setup_choose
setup_validate

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

install_system() {
    local message=$1
    shift
    if [[ ${LUDASH_PREFER_PKEXEC:-0} == 1 ]]; then
        report_progress 82 authorization "Approve the system authorization request to continue installation."
        # Emit this marker only after pkexec has authorized the operation. Keep
        # progress-file writes in the unprivileged reader, not the root command.
        if ! run "${elevate[@]}" /bin/sh -c \
            'printf "%s\n" LUNADASH_INSTALL_AUTHORIZED; exec "$@"' \
            lunadash-install "$@" </dev/null 2>&1 | while IFS= read -r line; do
                if [[ $line == LUNADASH_INSTALL_AUTHORIZED ]]; then
                    report_progress 84 install "$message"
                else
                    printf '%s\n' "$line"
                fi
            done; then
            echo 'System installation failed or authorization was cancelled/unavailable. Use a desktop polkit agent, or run ./scripts/install-session.sh from a terminal.' >&2
            return 1
        fi
    else
        report_progress 84 install "$message"
        run "${elevate[@]}" "$@"
    fi
}

# Background updates must not wait on a hidden package/password prompt. The
# graphical path uses the registered desktop authentication agent instead.
$non_interactive && exec </dev/null

# Start before building so registration can finish before privilege elevation.
# An already registered session agent remains authoritative; duplicate agents
# exit on registration failure. Only the child started here is cleaned up.
auth_agent_pid=0
cleanup_auth_agent() {
    if (( auth_agent_pid > 0 )); then
        kill "$auth_agent_pid" 2>/dev/null || true
        wait "$auth_agent_pid" 2>/dev/null || true
    fi
}
if [[ ${LUDASH_PREFER_PKEXEC:-0} == 1 ]] && ! $dry_run; then
    if "$project_dir/scripts/lunadash-polkit-agent" --print >/dev/null 2>&1; then
        "$project_dir/scripts/lunadash-polkit-agent" &
        auth_agent_pid=$!
        trap cleanup_auth_agent EXIT
        trap 'exit 130' HUP INT TERM
    fi
fi

report_progress 36 prepare "Preparing the LunaDash installation script."

if ! $skip_deps; then
    report_progress 38 dependencies "Installing or verifying build dependencies."
    deps=(bash "$project_dir/scripts/install-dependencies.sh")
    $dry_run && deps+=(--dry-run)
    run "${deps[@]}"
fi

setup_install_apps

if [[ $package_manager == pacman ]]; then
    command -v makepkg >/dev/null 2>&1 || { echo 'makepkg is required on Arch Linux.' >&2; exit 1; }
    report_progress 42 package "Preparing the Arch Linux package source."
    run "$project_dir/scripts/make-source.sh"
    (
        cd -- "$project_dir/packaging/arch"
        if $non_interactive || [[ ${LUDASH_PREFER_PKEXEC:-0} == 1 ]]; then
            report_progress 54 build "Building the Arch Linux package."
            run makepkg --force --cleanbuild --noconfirm
            mapfile -t packages < <(makepkg --packagelist)
            (("${#packages[@]}" > 0)) || { echo 'makepkg produced no package paths.' >&2; exit 1; }
            if ! $dry_run; then
                for package in "${packages[@]}"; do
                    [[ -f $package ]] || { echo "Built package is missing: $package" >&2; exit 1; }
                done
            fi
            install_system "Installing the newly built LunaDash package." pacman -U --noconfirm "${packages[@]}"
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
    install_system "Installing LunaDash system files." cmake --install "$build_dir"
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

setup_write_profile

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
auto-login file from a TTY if needed. See docs/en/LOGIN_SESSION.md (English) or docs/zh/LOGIN_SESSION.md (繁體中文).
EOF
