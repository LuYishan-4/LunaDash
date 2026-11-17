#!/usr/bin/env bash
# LunaDash installer for Arch Linux. Works from a file or curl ... | bash.
#
# Piped from curl it is a bootstrap: it fetches the LunaDash source into
# ~/.cache/lunadash/source, links the `lunadash` command into ~/.local/bin and
# continues with the install.sh of that checkout. Afterwards `lunadash update`
# and `lunadash install` work from the same cache.
# Run from a LunaDash checkout it installs that checkout as it is.
#
# All questions and child-process input use /dev/tty, never the script stream.
set -Eeuo pipefail

ui=en
stage=preflight
work=
log_file=
dry_run=false
update_mode=false
requested_language=
source_ref=
source_dir=
# Desktop shell for LunaDash sessions (session.json): lunadash, custom or none.
# Empty keeps the existing choice; without one, sessions start the LunaDash shell.
shell_choice=
shell_command=
sddm_enabled=false
readonly repository=LuYishan-4/LunaDash
readonly official_url="https://github.com/$repository.git"
readonly default_ref=dev
readonly -a base_packages=(base base-devel glibc git curl wget ca-certificates
    openssh unzip zip tar xz gzip bzip2 zstd rsync nano less man-db man-pages
    which file findutils grep sed gawk diffutils util-linux procps-ng iproute2
    iputils networkmanager wireless-regdb linux-firmware pciutils usbutils
    xdg-user-dirs xdg-utils dbus sudo polkit jq python cmake ninja pkgconf
    fontconfig noto-fonts noto-fonts-emoji)
readonly -a app_packages=(google-chrome discord zed visual-studio-code-bin
    obs-studio spotify protonplus kitty fish dolphin filezilla htop nmap tor)
readonly -a app_names=("Google Chrome" "Discord" "Zed" "Visual Studio Code"
    "OBS Studio" "Spotify" "ProtonPlus" "Kitty" "Fish" "Dolphin" "FileZilla"
    "htop" "Nmap" "Tor")
readonly -a app_sources=(AUR repository repository AUR repository AUR auto
    repository repository repository repository repository repository repository)
readonly -a flatpak_ids=(com.google.Chrome com.discordapp.Discord dev.zed.Zed
    com.visualstudio.code com.obsproject.Studio com.spotify.Client
    com.vysp3r.ProtonPlus '' '' org.kde.dolphin org.filezillaproject.Filezilla '' '' '')
selected=()

say() { if [[ $ui == zh_TW ]]; then printf '%s\n' "$2"; else printf '%s\n' "$1"; fi; }
die() { say "$1" "$2" >&2; exit 1; }
warn() { say "Warning: $1" "警告：$2" >&2; }
ask() {
    say "$1" "$2"
    printf '> '
    IFS= read -r answer <&3 || die 'Input closed; installation cancelled.' '輸入已關閉，安裝已取消。'
}
yes_no() {
    while true; do
        ask "$1 [y/N]" "$2 [y/N]"
        case "${answer,,}" in
            y|yes|是) return 0 ;;
            ''|n|no|否) return 1 ;;
            *) say 'Enter y or n.' '請輸入 y 或 n。' ;;
        esac
    done
}
run() {
    printf '+ '; printf '%q ' "$@"; printf '\n'
    if ! $dry_run; then "$@" <&3; fi
}
as_root() { run sudo -- "$@"; }
installed() {
    command -v pacman >/dev/null 2>&1 && pacman -Qq "$1" >/dev/null 2>&1
}
app_installed() {
    installed "${app_packages[$1]}" && return 0
    local id=${flatpak_ids[$1]}
    [[ -n $id ]] && command -v flatpak >/dev/null 2>&1 && flatpak info "$id" >/dev/null 2>&1
}
finish() {
    local status=$?
    trap - EXIT
    if ((status)); then
        say "Installation stopped at: $stage (exit $status). No reboot was requested." \
            "安裝在 $stage 階段停止（代碼 $status），不會重新開機。" >&2
        [[ -z $log_file ]] || printf 'Log: %s\n' "$log_file" >&2
    fi
    if [[ -n $work && -d $work && ${work##*/} == lunadash-install.* ]]; then
        rm -rf -- "$work"
    fi
    exit "$status"
}
help() {
    cat <<'HELP'
LunaDash installer (Arch Linux x86_64)
Usage: install.sh [--dry-run] [--update] [--language en|zh_TW] [--ref BRANCH_OR_SHA]
                  [--shell lunadash|custom|none] [--shell-command CMD]

curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash

Requires a running installed system, Internet access, Bash, curl and a terminal.
Run as a normal user with sudo permission. The root account is never used to
build AUR packages. Review this script before running code fetched from a URL.

The source is kept in ~/.cache/lunadash/source and `lunadash` is linked into
~/.local/bin. Afterwards:
  lunadash update     fetch the newest source of the same branch and rebuild
  lunadash install    run the guided installation again
Run from a LunaDash checkout, install.sh installs that checkout instead.

--dry-run   print the plan without downloads, package installs or config writes.
--update    only upgrade the system and rebuild/reinstall LunaDash, without the
            installer's questions (pacman still confirms the system upgrade).
--language  installer language; by default it follows LANG (Chinese or English).
            It never sets LANG, the system locale, keyboard layout or timezone.
--ref       branch, tag or commit to install (default: dev, or the cached one).
--shell     what LunaDash sessions start besides the compositor: the LunaDash
            shell (default), your own command (--shell-command, e.g. waybar) or
            none. Written to ~/.config/LunaDash/session.json.

If GitHub cannot be reached the source is fetched through the gh-proxy.org
mirror. LUNADASH_REPO=URL uses only that repository (a fork or local mirror).
The existing scripts/install-session.sh remains the developer/testing entry.
HELP
}
banner() {
    local color='' reset=''
    if [[ -t 1 && ${TERM:-dumb} != dumb && -z ${NO_COLOR:-} ]]; then
        color=$'\033[1;36m'; reset=$'\033[0m'
    fi
    printf '%s\n' "$color"
    cat <<'BANNER'
 ██╗     ██╗   ██╗███╗   ██╗ █████╗ ██████╗  █████╗ ███████╗██╗  ██╗
 ██║     ██║   ██║████╗  ██║██╔══██╗██╔══██╗██╔══██╗██╔════╝██║  ██║
 ██║     ██║   ██║██╔██╗ ██║███████║██║  ██║███████║███████╗███████║
 ██║     ██║   ██║██║╚██╗██║██╔══██║██║  ██║██╔══██║╚════██║██╔══██║
 ███████╗╚██████╔╝██║ ╚████║██║  ██║██████╔╝██║  ██║███████║██║  ██║
 ╚══════╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝  ╚═╝╚═════╝ ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝
   ░░░░░░░  ░░░░░░  ░░  ░░░░ ░░   ░░ ░░░░░░  ░░   ░░ ░░░░░░░ ░░   ░░
BANNER
    printf '%s\n' "$reset"
    printf '  LunaDash · Wayland desktop / 桌面環境\n\n'
}
# The installer speaks Traditional Chinese or English; any Chinese locale picks Chinese.
detect_language() {
    if [[ -n $requested_language ]]; then ui=$requested_language; return; fi
    local locale=${LC_ALL:-${LC_MESSAGES:-${LANG:-}}}
    if [[ ${locale,,} == zh* ]]; then ui=zh_TW; else ui=en; fi
}

# --- Source cache -----------------------------------------------------------------

cache_dir() {
    local base=${XDG_CACHE_HOME:-}
    [[ $base == /* ]] || base="$HOME/.cache"
    printf '%s/lunadash/source\n' "$base"
}
is_checkout() {
    [[ -n $1 && -f $1/CMakeLists.txt && -f $1/packaging/arch/PKGBUILD &&
        -f $1/scripts/make-source.sh && -f $1/install.sh ]]
}
# Git sources in order of preference. A mirror only carries bytes: every fetch is
# checked against a commit id, which Git verifies against the content.
git_sources() {
    if [[ -n ${LUNADASH_REPO:-} ]]; then
        printf '%s\n' "$LUNADASH_REPO"
    else
        printf '%s\n' "$official_url" "https://gh-proxy.org/$official_url"
    fi
}
git_quiet() {
    env GIT_TERMINAL_PROMPT=0 git -c maintenance.auto=false \
        -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=20 "$@"
}
# Prints the commit of $1 (branch, tag or commit id).
resolve_revision() {
    local ref=$1 url lines sha
    if [[ $ref =~ ^[0-9a-f]{40}$ ]]; then printf '%s\n' "$ref"; return; fi
    while IFS= read -r url; do
        lines=$(git_quiet ls-remote "$url" "refs/heads/$ref" "refs/tags/$ref" 2>/dev/null) || continue
        [[ -n $lines ]] || continue
        # An annotated tag lists its peeled commit as refs/tags/NAME^{}.
        sha=$(awk -v peeled="refs/tags/$ref^{}" '$2 == peeled { print $1; exit }' <<< "$lines")
        [[ -n $sha ]] || sha=$(awk 'NR == 1 { print $1 }' <<< "$lines")
        [[ $sha =~ ^[0-9a-f]{40}$ ]] || continue
        if [[ -z ${LUNADASH_REPO:-} && $url != "$official_url" ]]; then
            warn "GitHub is unreachable; $ref was resolved through $url and could not be confirmed with GitHub." \
                 "無法連線 GitHub；$ref 的版本是透過 $url 取得，無法向 GitHub 確認。"
        fi
        printf '%s\n' "$sha"
        return
    done < <(git_sources)
    return 1
}
refresh_cache() {
    local cache=$1 ref=$2 revision url fetched=false
    stage=source-fetch
    say ":: Fetching LunaDash $ref into $cache" ":: 下載 LunaDash $ref 到 $cache"
    if [[ -d $cache ]] && ! git -C "$cache" rev-parse --git-dir >/dev/null 2>&1; then
        # Only ever delete the cache directory this installer owns.
        [[ $cache == */lunadash/source ]] || die "Refusing to replace $cache." "拒絕取代 $cache。"
        rm -rf -- "$cache"
    fi
    if [[ ! -d $cache ]]; then
        mkdir -p -- "${cache%/*}"
        git_quiet init --quiet "$cache"
        git -C "$cache" remote add origin "$official_url"
    elif [[ -n $(git -C "$cache" status --porcelain --untracked-files=no) ]]; then
        die "$cache has local changes. Commit, stash or remove them, then run again." \
            "$cache 有本機修改，請先提交、暫存或移除後再執行。"
    fi
    revision=$(resolve_revision "$ref") || die \
        "Cannot reach GitHub or its mirror to resolve $ref. Check the network and try again." \
        "無法連線 GitHub 或鏡像來取得 $ref，請檢查網路後重試。"
    while IFS= read -r url; do
        say "   from $url" "   來源 $url"
        if git_quiet -C "$cache" fetch --quiet --no-tags --depth=1 "$url" "$revision"; then
            fetched=true; break
        fi
    done < <(git_sources)
    $fetched || die 'Downloading the LunaDash source failed from every source.' '所有來源都無法下載 LunaDash 原始碼。'
    git -C "$cache" checkout --quiet --detach FETCH_HEAD
    [[ $(git -C "$cache" rev-parse HEAD) == "$revision" ]] || \
        die 'Downloaded source does not match the requested revision.' '下載的原始碼與要求的版本不符。'
    git -C "$cache" config lunadash.ref "$ref"
    # Shallow fetches pile up old snapshots; keep only the checked-out one.
    git -C "$cache" reflog expire --expire=now --all
    git -C "$cache" gc --quiet --prune=now
    say "   LunaDash $revision" "   LunaDash $revision"
}
link_command() {
    local cache=$1 bin="$HOME/.local/bin"
    local target="$cache/scripts/lunadash" link="$bin/lunadash"
    if [[ -e $link || -L $link ]] && [[ $(readlink -- "$link" || true) != "$target" ]]; then
        warn "$link already exists and is kept; run $target instead of lunadash." \
             "$link 已存在，保留不動；請改用 $target。"
        return
    fi
    mkdir -p -- "$bin"
    ln -sfn -- "$target" "$link"
    case ":$PATH:" in
        *":$bin:"*) ;;
        *) say "Add $bin to PATH to use the lunadash command." "請把 $bin 加入 PATH 才能使用 lunadash 指令。" ;;
    esac
}
ensure_git() {
    command -v git >/dev/null 2>&1 && return
    say 'Git is needed to fetch LunaDash; installing it with pacman.' '需要 Git 才能下載 LunaDash，將以 pacman 安裝。'
    command -v sudo >/dev/null 2>&1 || die 'Install git and sudo first, for example: su -c "pacman -S git sudo"' \
        '請先安裝 git 與 sudo，例如：su -c "pacman -S git sudo"'
    sudo pacman -S --needed git <&3
}
# curl | bash and standalone copies land here: fetch the source and continue with it.
bootstrap_source() {
    local cache
    cache=$(cache_dir)
    if $dry_run; then
        printf '+ Fetch %s@%s into %s (GitHub, then gh-proxy.org)\n' "$repository" "${source_ref:-$default_ref}" "$cache"
        printf '+ Link %s/.local/bin/lunadash -> %s/scripts/lunadash\n' "$HOME" "$cache"
        return
    fi
    ensure_git
    refresh_cache "$cache" "${source_ref:-$default_ref}"
    link_command "$cache"
    exec env LUNADASH_BOOTSTRAPPED=1 bash "$cache/install.sh" --language "$ui" "${original_args[@]}" <&3
}

# --- Installation steps -----------------------------------------------------------

check_system() {
    ((EUID != 0)) || die 'Run as a normal user, not root or sudo bash.' '請以一般使用者執行，不要使用 root 或 sudo bash。'
    [[ -r /etc/os-release ]] || die 'Cannot identify this system.' '無法辨識作業系統。'
    local ID='' ID_LIKE=''
    # os-release is an operating-system-owned configuration file.
    . /etc/os-release
    [[ ${ID:-} == arch || " ${ID_LIKE:-} " == *' arch '* ]] || \
        die 'This installer supports Arch-based systems. Use the documented source install for other distributions.' \
            '此正式安裝程式支援 Arch 系統；其他發行版請使用文件中的原始碼安裝流程。'
    [[ $(uname -m) == x86_64 ]] || die 'The guided application set currently requires x86_64.' '此常用軟體安裝組合目前需要 x86_64。'
    command -v pacman >/dev/null || die 'pacman is required.' '需要 pacman。'
}
preflight() {
    if $dry_run; then
        say 'DRY RUN: Arch Linux plan only; nothing will be changed.' '預覽模式：僅顯示 Arch Linux 安裝計畫，不會修改系統。'
        return
    fi
    if ! command -v sudo >/dev/null 2>&1; then
        say 'sudo is missing. Root authentication will install sudo, without changing sudoers.' \
            '尚未安裝 sudo，將透過 root 驗證安裝 sudo；不會修改 sudoers 授權。'
        command -v su >/dev/null || die 'Install sudo and grant your user permission first.' '請先安裝 sudo 並授權目前使用者。'
        su -s /bin/bash -c 'pacman -Syu --needed sudo' <&3
    fi
    sudo -v <&3 || die 'Your account needs sudo permission. No sudoers rules were changed.' \
        '目前帳號需要 sudo 權限；安裝程式未修改任何 sudoers 規則。'
    umask 077
    local state=${XDG_STATE_HOME:-"$HOME/.local/state"}/lunadash
    mkdir -p -- "$state"
    log_file=$(mktemp "$state/install-XXXXXXXX")
    exec > >(tee -a "$log_file") 2>&1
    work=$(mktemp -d "${TMPDIR:-/tmp}/lunadash-install.XXXXXXXX")
}
install_base_packages() {
    stage=base-packages
    say 'Step 1/5 · Basic packages and a full system upgrade (pacman asks before it changes anything)' \
        '步驟 1/5 · 基本軟體與完整系統更新（pacman 變更前會先詢問）'
    printf '%s\n' "${base_packages[*]}"
    as_root pacman -Syu --needed "${base_packages[@]}"
}
ensure_yay() {
    command -v yay >/dev/null 2>&1 && return
    stage=aur-helper
    say 'The selected AUR applications need yay. AUR build scripts are community code, not official Arch packages.' \
        '選取的 AUR 軟體需要 yay。AUR 建置腳本是社群程式碼，不是 Arch 官方套件。'
    if $dry_run; then
        printf '+ Fetch yay from the AUR, show PKGBUILD, makepkg --syncdeps --install (unprivileged build)\n'
        return
    fi
    run git -c maintenance.auto=false clone --depth 1 https://aur.archlinux.org/yay.git "$work/yay" || return 1
    cat -- "$work/yay/PKGBUILD" "$work/yay/.SRCINFO" || return 1
    yes_no 'Build and install yay from the recipe above?' '依上面的建置腳本建置並安裝 yay？' || return 1
    (cd -- "$work/yay" && run makepkg --syncdeps --install)
}
print_apps() {
    local i mark status
    for i in "${!app_packages[@]}"; do
        mark=' '; status=${app_sources[$i]}
        [[ ${selected[$i]} == 1 ]] && mark=x
        if app_installed "$i"; then status=$(say 'installed' '已安裝'); fi
        printf ' %2d [%s] %-22s [%s]\n' "$((i + 1))" "$mark" "${app_names[$i]}" "$status"
    done
}
choose_apps() {
    stage=applications
    say 'Step 2/5 · Optional everyday applications' '步驟 2/5 · 選擇常用軟體'
    selected=()
    for _ in "${app_packages[@]}"; do selected+=(0); done
    local i token valid
    while true; do
        print_apps
        ask 'Numbers toggle, all / none, -N excludes (all -3 -5). Press Enter to continue.' \
            '輸入數字切換、all 全選、none 清空、-數字排除（all -3 -5）。直接按 Enter 繼續。'
        [[ -z ${answer// } || $answer == done ]] && break
        local -a tokens=()
        read -ra tokens <<< "$answer"
        valid=true
        for token in "${tokens[@]}"; do
            if [[ $token != all && $token != none && ! $token =~ ^-?([1-9]|1[0-4])$ ]]; then valid=false; fi
        done
        if ! $valid; then
            say 'Invalid selection; no choices changed.' '選項無效，未變更選擇。'; continue
        fi
        for token in "${tokens[@]}"; do
            case "$token" in
                all) for i in "${!selected[@]}"; do selected[$i]=1; done ;;
                none) for i in "${!selected[@]}"; do selected[$i]=0; done ;;
                -*) selected[$((-token - 1))]=0 ;;
                *) i=$((token - 1)); selected[$i]=$((1 - selected[i])) ;;
            esac
        done
    done
    local -a official=() aur=()
    for i in "${!app_packages[@]}"; do
        [[ ${selected[$i]} == 1 ]] || continue
        app_installed "$i" && continue
        local package=${app_packages[$i]} source=${app_sources[$i]}
        if [[ $source == auto ]]; then
            if ! $dry_run && pacman -Si "$package" >/dev/null 2>&1; then source=repository
            else source=AUR; fi
        fi
        if [[ $source == AUR ]]; then aur+=("$package"); else official+=("$package"); fi
    done
    ((${#official[@]} + ${#aur[@]})) || { say 'No additional applications selected.' '未選取需要新增安裝的軟體。'; return; }
    # pacman and yay list the packages and ask before installing them.
    if ((${#official[@]})); then as_root pacman -S --needed "${official[@]}"; fi
    if ((${#aur[@]})); then
        if ensure_yay; then
            run yay -S --needed "${aur[@]}"
        else
            say "Skipped AUR applications: ${aur[*]}" "已略過 AUR 軟體：${aur[*]}"
        fi
    fi
}
write_shell_choice() {
    [[ -n $shell_choice ]] || return 0
    stage=shell-config
    if $dry_run; then
        printf '+ Write desktop shell "%s" to ~/.config/LunaDash/session.json\n' "$shell_choice"
        return
    fi
    local -a command=(python3 "$source_dir/scripts/setup-profile.py" --shell "$shell_choice")
    [[ -z $shell_command ]] || command+=(--shell-command "$shell_command")
    run "${command[@]}"
}
build_desktop() {
    stage=desktop-build
    say 'Step 3/5 · LunaDash desktop (compositor, LunaDash shell and SlintLu)' \
        '步驟 3/5 · LunaDash 桌面（合成器、LunaDash 殼層與 SlintLu）'
    if $dry_run; then
        printf '+ make-source.sh: fetch LunaDash-Shell and SlintLu at packaging/*-revision\n'
        printf '+ makepkg --syncdeps --force; sudo pacman -U (built package)\n'
        return
    fi
    printf 'Source: %s (%s)\n' "$source_dir" "$(git -C "$source_dir" rev-parse HEAD 2>/dev/null || cat "$source_dir/.lunadash-revision")"
    run bash "$source_dir/scripts/make-source.sh"
    (
        cd -- "$source_dir/packaging/arch"
        run makepkg --syncdeps --force
        local -a packages=()
        mapfile -t packages < <(makepkg --packagelist)
        ((${#packages[@]})) || exit 1
        for package in "${packages[@]}"; do [[ -f $package ]] || exit 1; done
        # The package was built from the source chosen above; no second confirmation.
        # Not --needed: an update rebuilds the same version number from newer source.
        as_root pacman -U --noconfirm "${packages[@]}"
    )
    if [[ -e /usr/local/bin/lunadash-compositor ]]; then
        say 'Warning: an older LunaDash under /usr/local (manual CMake install) takes precedence over this package at login. Remove its files from /usr/local.' \
            '警告：/usr/local 底下有舊版 LunaDash（手動 CMake 安裝），登入時會蓋過這個套件。請移除 /usr/local 底下的 LunaDash 檔案。' >&2
    fi
    if systemctl --user show-environment >/dev/null 2>&1; then
        run systemctl --user daemon-reload
    else
        say 'No user service manager is active; portal units will be loaded at the next login.' \
            '目前沒有使用者服務管理器，portal 服務將於下次登入時載入。'
    fi
}
choose_locale() {
    stage=system-language
    say 'Step 4/5 · System language (independent of the installer language)' \
        '步驟 4/5 · 系統語言（與安裝程式語言分開設定）'
    local -a locales=(keep zh_TW.UTF-8 en_US.UTF-8 zh_CN.UTF-8 ja_JP.UTF-8 ko_KR.UTF-8
        de_DE.UTF-8 fr_FR.UTF-8 es_ES.UTF-8 pt_BR.UTF-8)
    local -a names=("Keep current (${LANG:-unset}) / 保留目前設定" "Taiwan / 繁體中文" "United States / English"
        "China / 简体中文" "Japan / 日本語" "Korea / 한국어" "Germany / Deutsch"
        "France / Français" "Spain / Español" "Brazil / Português")
    for i in "${!locales[@]}"; do printf ' %2d) %s\n' "$i" "${names[$i]}"; done
    say ' 10) Other UTF-8 locale from /usr/share/i18n/SUPPORTED' ' 10) 其他 UTF-8 語系（參考 /usr/share/i18n/SUPPORTED）'
    local locale_code=
    while true; do
        ask 'Choose system language [0-10; Enter keeps the current one]:' '選擇系統語言 [0-10，直接按 Enter 保留目前設定]：'
        answer=${answer:-0}
        if [[ $answer =~ ^[0-9]$ ]]; then locale_code=${locales[$answer]}; break; fi
        if [[ $answer == 10 ]]; then
            ask 'Enter a UTF-8 locale, for example en_GB.UTF-8:' '輸入 UTF-8 語系，例如 en_GB.UTF-8：'
            if [[ $answer =~ ^[a-z]{2,3}_[A-Z]{2}\.UTF-8$ ]]; then locale_code=$answer; break; fi
        fi
        say 'Invalid locale selection.' '語系選項無效。'
    done
    local font_locale=$locale_code
    [[ $font_locale != keep ]] || font_locale=${LANG:-en_US.UTF-8}
    local -a fonts=(noto-fonts noto-fonts-emoji ttf-dejavu fcitx5-gtk)
    case "$font_locale" in
        zh_TW*|zh_HK*) fonts+=(noto-fonts-cjk fcitx5-chewing) ;;
        zh_*) fonts+=(noto-fonts-cjk fcitx5-chinese-addons) ;;
        ja_*) fonts+=(noto-fonts-cjk fcitx5-mozc) ;;
        ko_*) fonts+=(noto-fonts-cjk fcitx5-hangul) ;;
        en_*|de_*|fr_*|es_*|pt_*) ;;
        *) fonts+=(noto-fonts-extra) ;;
    esac
    say "System locale: $locale_code · keyboard layout and timezone stay unchanged" \
        "系統語系：$locale_code · 鍵盤配置與時區保持不變"
    say "Fonts and input methods: ${fonts[*]}" "字型與輸入法：${fonts[*]}"
    as_root pacman -S --needed "${fonts[@]}"
    if [[ $locale_code != keep ]]; then
        if $dry_run; then
            printf '+ Enable %q in locale.gen with backup; locale-gen; localectl set-locale LANG=%q\n' "$locale_code" "$locale_code"
        else
            # Pass the locale as data, never as an interpolated shell command.
            cat > "$work/locale.py" <<'PY'
import os, pathlib, re, shutil, sys, tempfile, time
locale = sys.argv[1]
if not re.fullmatch(r'[a-z]{2,3}_[A-Z]{2}\.UTF-8', locale):
    raise SystemExit('Invalid locale')
supported = pathlib.Path('/usr/share/i18n/SUPPORTED').read_text().splitlines()
if not any(line.split() == [locale, 'UTF-8'] for line in supported):
    raise SystemExit('Locale is not supported by the installed glibc')
path = pathlib.Path('/etc/locale.gen')
text = path.read_text()
pattern = re.compile(r'^\s*#?\s*' + re.escape(locale) + r'\s+UTF-8\s*$', re.M)
updated = pattern.sub(locale + ' UTF-8', text) if pattern.search(text) else text + '\n' + locale + ' UTF-8\n'
if updated != text:
    shutil.copy2(path, str(path) + '.lunadash-backup.' + str(time.time_ns()))
    fd, temporary = tempfile.mkstemp(prefix='.lunadash-locale-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as stream:
            stream.write(updated)
            stream.flush()
            os.fsync(stream.fileno())
        os.chmod(temporary, path.stat().st_mode & 0o777)
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)
PY
            as_root python3 "$work/locale.py" "$locale_code"
            as_root locale-gen
            as_root localectl set-locale "LANG=$locale_code"
        fi
        local shell_locale=${locale_code%%.*}
        case "$shell_locale" in
            zh_TW|zh_CN|ja_JP|en_US) ;;
            *) shell_locale=en_US
                say 'The system locale is kept; LunaDash currently falls back to English for this UI language.' \
                    '系統語系依您的選擇設定；LunaDash 尚無此介面翻譯，介面暫時使用英文。' ;;
        esac
        run lunadash-compositor --set-language "$shell_locale"
    fi
    run fc-cache -f
}
# A network service other than NetworkManager that is already configured.
other_network_service() {
    local service
    for service in systemd-networkd connman iwd dhcpcd; do
        systemctl is-enabled "$service.service" >/dev/null 2>&1 && return 0
    done
    return 1
}
configure_login() {
    stage=login
    say 'Step 5/5 · Wallpaper library, login screen and network' '步驟 5/5 · 桌布庫、登入畫面與網路'
    if $dry_run; then
        say 'Plan: create the standard Pictures/Wallpapers directory only when missing.' \
            '計畫：僅在不存在時建立標準 Pictures/Wallpapers 桌布資料夾。'
    else
        run xdg-user-dirs-update
        local pictures
        pictures=$(xdg-user-dir PICTURES)
        [[ -n $pictures && $pictures == /* ]] || pictures="$HOME/Pictures"
        run mkdir -p -- "$pictures/Wallpapers"
    fi
    if [[ -e /etc/systemd/system/display-manager.service ]]; then
        say 'Keeping your existing display manager.' '保留目前的登入管理器。'
    elif yes_no 'No login screen is set up. Install and enable SDDM for the next boot? The current session keeps running.' \
                 '尚未設定登入畫面。安裝並啟用 SDDM，於下次開機使用？目前的工作階段不受影響。'; then
        as_root pacman -S --needed sddm
        as_root systemctl enable sddm.service
        as_root systemctl set-default graphical.target
        sddm_enabled=true
    fi
    if ! command -v systemctl >/dev/null 2>&1; then
        :
    elif systemctl is-enabled NetworkManager.service >/dev/null 2>&1; then
        :
    elif other_network_service; then
        say 'Another network service is configured; its configuration is unchanged.' '已設定其他網路服務，保持其設定不變。'
    else
        say 'Enabling NetworkManager from the next boot; the current connection is unchanged.' \
            '下次開機起啟用 NetworkManager；目前的連線不受影響。'
        as_root systemctl enable NetworkManager.service
    fi
}
finish_message() {
    if $dry_run; then
        say 'Preview complete; nothing was installed.' '預覽完成，未安裝任何內容。'
        return
    fi
    if $update_mode; then
        say 'LunaDash is updated. Log out and back in to use the new version.' \
            'LunaDash 已更新。登出後重新登入即可使用新版本。'
    elif $sddm_enabled; then
        say 'Installation complete. Reboot to reach the SDDM login screen and choose LunaDash: sudo systemctl reboot' \
            '安裝完成。重新開機後會出現 SDDM 登入畫面，請選擇 LunaDash：sudo systemctl reboot'
    else
        say 'Installation complete. Log out and choose LunaDash in your login manager. Super+W opens the wallpaper gallery.' \
            '安裝完成。登出後在登入管理器選擇 LunaDash；Super+W 可開啟桌布選單。'
    fi
    if [[ -L $HOME/.local/bin/lunadash ]]; then
        say 'Later: lunadash update (newest source) · lunadash install (run this guide again)' \
            '之後：lunadash update（更新到最新原始碼）· lunadash install（重新執行安裝導覽）'
    fi
    [[ -z $log_file ]] || printf 'Log: %s\n' "$log_file"
}
parse_args() {
    while (($#)); do
        case "$1" in
            --help|-h) help; exit 0 ;;
            --dry-run) dry_run=true ;;
            --update) update_mode=true ;;
            --language)
                (($# >= 2)) || { help >&2; exit 2; }
                requested_language=$2; shift
                [[ $requested_language == en || $requested_language == zh_TW ]] || exit 2 ;;
            --shell)
                (($# >= 2)) || { help >&2; exit 2; }
                shell_choice=$2; shift
                [[ $shell_choice == lunadash || $shell_choice == custom || $shell_choice == none ]] || exit 2 ;;
            --shell-command)
                (($# >= 2)) && [[ -n $2 ]] || { help >&2; exit 2; }
                shell_command=$2; shift ;;
            --ref)
                (($# >= 2)) || exit 2
                source_ref=$2; shift
                [[ $source_ref =~ ^[A-Za-z0-9][A-Za-z0-9._/-]*$ && $source_ref != *..* ]] || exit 2 ;;
            *) help >&2; exit 2 ;;
        esac
        shift
    done
    if [[ -n $shell_command && $shell_choice != custom ]]; then
        printf -- '--shell-command needs --shell custom.\n' >&2
        exit 2
    fi
    if [[ $shell_choice == custom && -z $shell_command ]]; then
        printf -- '--shell custom needs --shell-command.\n' >&2
        exit 2
    fi
    if [[ -n ${LUNADASH_REPO:-} && ! $LUNADASH_REPO =~ ^(https://|ssh://|git@|/) ]]; then
        printf 'LUNADASH_REPO must start with https://, ssh://, git@ or /.\n' >&2
        exit 2
    fi
}
main() {
    original_args=("$@")
    parse_args "$@"
    # Open this before any prompt. No fallback to stdin: it may be the curl pipe.
    if ! { exec 3</dev/tty; } 2>/dev/null; then
        printf 'A terminal is required / 需要互動終端。Download the script and run it in a terminal.\n' >&2
        return 1
    fi
    trap finish EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    detect_language

    local script_dir=''
    if [[ -n ${BASH_SOURCE[0]:-} && -f ${BASH_SOURCE[0]} ]]; then
        script_dir=$(cd -- "$(dirname -- "$(readlink -f -- "${BASH_SOURCE[0]}")")" && pwd)
    fi
    if [[ ${LUNADASH_BOOTSTRAPPED:-} != 1 ]]; then
        banner
        $dry_run || check_system
    fi
    if is_checkout "$script_dir"; then
        source_dir=$script_dir
        if [[ $script_dir == "$(cache_dir)" ]]; then
            # The installer cache: bring it to the wanted ref, then run its newest installer.
            if [[ ${LUNADASH_BOOTSTRAPPED:-} != 1 ]] && ! $dry_run; then
                local ref=${source_ref:-$(git -C "$script_dir" config lunadash.ref || true)}
                refresh_cache "$script_dir" "${ref:-$default_ref}"
                link_command "$script_dir"
                exec env LUNADASH_BOOTSTRAPPED=1 bash "$script_dir/install.sh" --language "$ui" "${original_args[@]}" <&3
            fi
        elif [[ -n $source_ref ]]; then
            warn "--ref is ignored when installing the checkout $script_dir." "從 $script_dir 安裝時會忽略 --ref。"
        fi
    else
        bootstrap_source
    fi

    preflight
    if $update_mode; then
        stage=system-upgrade
        say 'Upgrading the system before rebuilding LunaDash against it.' '先更新系統，再依更新後的系統重建 LunaDash。'
        as_root pacman -Syu
        build_desktop
        write_shell_choice
        finish_message
        return
    fi
    install_base_packages
    choose_apps
    build_desktop
    write_shell_choice
    choose_locale
    configure_login
    finish_message
}
# Keep execution after the complete function definitions for curl-pipe use.
main "$@"
