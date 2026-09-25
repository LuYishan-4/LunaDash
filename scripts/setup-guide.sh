#!/usr/bin/env bash
# Sourced by install-session.sh; uses its run(), elevation and install options.

setup_ask() {
    local prompt=$1 default=$2 answer
    while true; do
        printf '%s [%s] ' "$prompt" "$([[ $default == yes ]] && printf Y/n || printf y/N)"
        IFS= read -r answer || { echo 'Setup cancelled before installation.' >&2; exit 1; }
        answer=${answer,,}
        [[ -n $answer ]] || answer=$default
        case "$answer" in
            y|yes) return 0 ;;
            n|no) return 1 ;;
            *) echo 'Enter y or n.' ;;
        esac
    done
}

setup_packages() {
    case "$1" in
        basics) printf '%s\n' xdg-utils xdg-user-dirs unzip zip rsync jq playerctl gnome-keyring ;;
        desktop) printf '%s\n' chromium ark kio-extras ;;
        media) printf '%s\n' mpv imv ffmpegthumbnailer ;;
        office) printf '%s\n' libreoffice-fresh libreoffice-fresh-zh-tw ;;
        development) printf '%s\n' git fish starship fastfetch eza fzf zoxide tmux btop ;;
        input) printf '%s\n' fcitx5-gtk fcitx5-chewing fcitx5-chinese-addons fcitx5-rime noto-fonts-cjk ttf-jetbrains-mono ttf-jetbrains-mono-nerd ;;
        *) return 1 ;;
    esac
}

setup_choose() {
    local first_install=true group summary
    if command -v lunadash-compositor >/dev/null 2>&1 ||
       command -v ludash-compositor >/dev/null 2>&1 ||
       [[ -e ${XDG_STATE_HOME:-$HOME/.local/state}/lunadash/setup/complete.json ]]; then
        first_install=false
    fi
    if [[ $guide_mode == always && ! -t 0 ]]; then
        echo '--guided requires a terminal. Use --skip-guide with explicit setup options for a scripted installation.' >&2
        exit 2
    fi
    if [[ $guide_mode != always ]]; then
        [[ $guide_mode == auto && -t 0 && -t 1 ]] || return 0
        $first_install && ! $non_interactive && ! $dry_run && ! $setup_requested || return 0
    fi
    setup_requested=true
    cat <<'EOF'

LunaDash 1.0.1a first-install guide
1. Install the LunaDash Wayland compositor, Quickshell UI and core dependencies.
2. Choose optional everyday apps and tools from configured Arch repositories.
3. Choose editable desktop and terminal templates. Personal settings stay intact.

The reference layout uses a floating top panel, a centered launcher and a right
dashboard. LunaDash remains the compositor; the reference Niri/Noctalia session
and its machine-specific system configuration are not installed.
EOF
    if [[ $package_manager == pacman ]]; then
        if [[ -z $setup_apps ]]; then
            for group in basics desktop media office development input; do
                case "$group" in
                    basics) summary='Basic utilities: archives, file transfer, clipboard helpers and keyring';;
                    desktop) summary='Everyday apps: Chromium, Ark and Dolphin network file support';;
                    media) summary='Media tools: MPV, image viewer and video thumbnails';;
                    office) summary='Office suite: LibreOffice with Traditional Chinese language support';;
                    development) summary='Terminal tools: Fish, Starship, Fastfetch, fuzzy search, tmux and btop';;
                    input) summary='Input and fonts: Fcitx5 Chinese engines, CJK fonts and JetBrains Mono';;
                esac
                printf '\n%s\nPackages: ' "$summary"
                setup_packages "$group" | tr '\n' ' '
                printf '\n'
                if setup_ask "Install $group?" "$([[ $group == basics || $group == desktop || $group == input ]] && printf yes || printf no)"; then
                    setup_apps+="${setup_apps:+,}$group"
                fi
            done
        fi
    else
        echo 'Optional app bundles currently target Arch. Core installation and editable profiles work on the other supported installer paths.'
    fi
    if ! $setup_desktop && setup_ask 'Create the editable LunaDash panel profile when missing?' yes; then
        setup_desktop=true
    fi
    if ! $setup_author && setup_ask 'Add portable author-inspired terminal templates when missing?' yes; then
        setup_author=true
    fi
    printf '\nInstallation: LunaDash core\nOptional groups: %s\nDesktop profile: %s\nTerminal templates: %s\n' \
        "${setup_apps:-none}" "$setup_desktop" "$setup_author"
    setup_ask 'Continue with these choices?' yes || { echo 'Setup cancelled before installation.'; exit 0; }
}

setup_validate() {
    local group
    setup_package_list=()
    [[ -n $setup_apps && $setup_apps != none ]] || return 0
    if [[ $package_manager != pacman ]]; then
        echo '--apps currently supports Arch/pacman only; install the documented equivalents on other distributions.' >&2
        exit 2
    fi
    if [[ $setup_apps == ,* || $setup_apps == *, || $setup_apps == *,,* ]]; then
        echo 'App groups must be a comma-separated list without empty entries.' >&2
        exit 2
    fi
    local -a groups packages
    IFS=, read -r -a groups <<< "$setup_apps"
    for group in "${groups[@]}"; do
        case "$group" in
            basics|desktop|media|office|development|input) ;;
            *) printf 'Unknown app group: %s\n' "$group" >&2; exit 2 ;;
        esac
        mapfile -t packages < <(setup_packages "$group")
        setup_package_list+=("${packages[@]}")
    done
}

setup_install_apps() {
    ((${#setup_package_list[@]})) || return 0
    # Ask pacman normally; no AUR bootstrap, repository edits or service changes.
    run "${elevate[@]}" pacman -S --needed -- "${setup_package_list[@]}"
}

setup_write_profile() {
    $setup_requested || return 0
    local -a args=(python3 "$project_dir/scripts/setup-profile.py" --complete --apps "${setup_apps:-none}")
    $setup_desktop && args+=(--desktop-profile)
    $setup_author && args+=(--author-config)
    run "${args[@]}"
}
