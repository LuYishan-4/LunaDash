#!/usr/bin/env bash
# Build a pacman-managed LuDash package and optionally configure boot login.
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
enable_sddm=false
autologin=""
dry_run=false
usage() {
    cat <<'EOF'
Usage: ./scripts/install-session.sh [--enable-sddm] [--autologin USER] [--dry-run]

Arch Linux: build and install a local LuDash package with pacman.
  --enable-sddm   Install/enable SDDM and graphical boot; never restart a desktop.
  --autologin USER
                  Opt into passwordless SDDM login for this user on boot.
                  Requires --enable-sddm. Existing auto-login config is preserved.
  --dry-run       Print the commands without building or modifying the system.

Run as your normal user. sudo/pacman will request authorization when needed.
LuDash is a development preview; physical GPU/seat/VT support is unverified.
Without auto-login, select LuDash (Wayland) in your login screen once.
EOF
}
while (($#)); do
    case "$1" in
        --enable-sddm) enable_sddm=true ;;
        --autologin) (($# >= 2)) && [[ -n $2 ]] || { usage >&2; exit 2; }; autologin=$2; shift ;;
        --dry-run) dry_run=true ;;
        --help|-h) usage; exit 0 ;;
        *) printf 'Unknown option: %s\n' "$1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done
if ((EUID == 0)); then echo 'Run as a normal user; makepkg must not run as root.' >&2; exit 1; fi
if ! command -v pacman >/dev/null; then
    echo 'This installer targets Arch Linux. See docs/LOGIN_SESSION.md for other distributions.' >&2
    exit 1
fi
if [[ -n $autologin ]]; then
    if ! $enable_sddm; then echo '--autologin requires --enable-sddm.' >&2; exit 2; fi
    if [[ ! $autologin =~ ^[a-z_][a-z0-9_-]*[$]?$ ]] || ! id "$autologin" >/dev/null 2>&1 || [[ $(id -u "$autologin") == 0 ]]; then
        echo 'Auto-login requires an existing, non-root local account name.' >&2; exit 2
    fi
    if [[ -e /etc/sddm.conf.d/90-ludash-autologin.conf || -L /etc/sddm.conf.d/90-ludash-autologin.conf ]]; then
        echo 'Auto-login configuration already exists. Review it manually before reinstalling with this option.' >&2; exit 1
    fi
fi
if $enable_sddm; then
    command -v systemctl >/dev/null || { echo 'SDDM setup requires systemd.' >&2; exit 1; }
    manager=$(readlink -f /etc/systemd/system/display-manager.service || true)
    if [[ -n $manager && -e $manager && ${manager##*/} != sddm.service ]]; then
        echo 'Another display manager is enabled. Run without --enable-sddm and select LuDash there.' >&2; exit 1
    fi
fi
run() {
    printf '+ '; printf '%q ' "$@"; printf '\n'
    if ! $dry_run; then "$@"; fi
}
run sudo pacman -S --needed base-devel cmake ninja
run "$project_dir/scripts/make-source.sh"
(
    cd -- "$project_dir/packaging/arch"
    run makepkg --syncdeps --force --install
)
if $enable_sddm; then
    run sudo pacman -S --needed sddm
    run sudo systemctl enable sddm.service
    run sudo systemctl set-default graphical.target
fi
if [[ -n $autologin ]]; then
    if $dry_run; then
        printf 'Would create /etc/sddm.conf.d/90-ludash-autologin.conf for %s, session ludash.desktop (Relogin=false).\n' "$autologin"
    else
        config=$(mktemp)
        trap 'rm -f -- "$config"' EXIT
        printf '[Autologin]\nUser=%s\nSession=ludash.desktop\nRelogin=false\n' "$autologin" > "$config"
        run sudo install -d -m 755 /etc/sddm.conf.d
        # No-clobber creation, including a symlink appearing after preflight.
        run sudo sh -c 'set -C; umask 022; cat "$1" > /etc/sddm.conf.d/90-ludash-autologin.conf' sh "$config"
    fi
fi
if $dry_run; then echo 'Dry run complete; no package or system setting was changed.'; exit 0; fi
cat <<'EOF'
Finished. Save your work, then log out or reboot when ready.
Choose LuDash (Wayland) in the login screen. The first-run guide opens on login.
Recovery: select your previous desktop; remove only the optional LuDash
auto-login file from a TTY if needed. See docs/LOGIN_SESSION.md.
EOF
