#!/usr/bin/env bash
# LunaDash interactive Arch installer. Works from a file or curl ... | bash.
# All questions and child-process input use /dev/tty, never the script stream.
set -Eeuo pipefail

ui=en
stage=preflight
work=
log_file=
dry_run=false
requested_language=
source_ref=dev
readonly repository=LuYishan-4/LunaDash
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
Usage: install.sh [--dry-run] [--language en|zh_TW] [--ref BRANCH_OR_SHA]

curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash

Requires a running installed system, Internet access, Bash, curl and a terminal.
Run as a normal user with sudo permission. The root account is never used to
build AUR packages. Review this script before running code fetched from a URL.
--dry-run prints the plan without downloads, package installs or config writes.
It still uses a terminal for the language, application and locale choices.
Script language does not set LANG, system locale, keyboard layout or timezone.
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
choose_language() {
    if [[ -n $requested_language ]]; then ui=$requested_language; return; fi
    while true; do
        printf '  1) 繁體中文\n  2) English\n\nSelect installer language / 選擇安裝程式語言 [1/2]: '
        IFS= read -r answer <&3 || exit 1
        case "$answer" in 1) ui=zh_TW; break ;; 2) ui=en; break ;; esac
    done
}
preflight() {
    if $dry_run; then
        say 'DRY RUN: Arch Linux plan only; nothing will be changed.' '預覽模式：僅顯示 Arch Linux 安裝計畫，不會修改系統。'
        return
    fi
    ((EUID != 0)) || die 'Run as a normal user, not root or sudo bash.' '請以一般使用者執行，不要使用 root 或 sudo bash。'
    [[ -r /etc/os-release ]] || die 'Cannot identify this system.' '無法辨識作業系統。'
    # os-release is an operating-system-owned configuration file.
    . /etc/os-release
    [[ ${ID:-} == arch || " ${ID_LIKE:-} " == *' arch '* ]] || \
        die 'This installer supports Arch-based systems. Use the documented source install for other distributions.' \
            '此正式安裝程式支援 Arch 系統；其他發行版請使用文件中的原始碼安裝流程。'
    [[ $(uname -m) == x86_64 ]] || die 'The guided application set currently requires x86_64.' '此常用軟體安裝組合目前需要 x86_64。'
    command -v pacman >/dev/null || die 'pacman is required.' '需要 pacman。'
    if ! command -v sudo >/dev/null 2>&1; then
        say 'sudo is missing. Root authentication will install sudo, without changing sudoers.' \
            '尚未安裝 sudo，將透過 root 驗證安裝 sudo；不會修改 sudoers 授權。'
        command -v su >/dev/null || die 'Install sudo and grant your user permission first.' '請先安裝 sudo 並授權目前使用者。'
        yes_no 'Install sudo with su now?' '現在透過 su 安裝 sudo？' || exit 0
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
bootstrap() {
    stage=base-packages
    say 'Step 1/5 · Basic packages (includes nmcli from NetworkManager)' '步驟 1/5 · 基本軟體（NetworkManager 包含 nmcli）'
    printf '%s\n' "${base_packages[*]}"
    say 'A full system upgrade is included to avoid an unsupported partial Arch upgrade.' \
        '包含完整系統更新，以避免 Arch 不支援的局部更新。'
    yes_no 'Install/update these basic packages?' '安裝或更新以上基本軟體？' || exit 0
    as_root pacman -Syu --needed "${base_packages[@]}"
    if ! command -v yay >/dev/null 2>&1; then
        stage=aur-helper
        say 'yay is an AUR helper. AUR build scripts are community code, not official Arch packages.' \
            'yay 是 AUR 助手。AUR 建置腳本是社群程式碼，不是 Arch 官方套件。'
        if $dry_run; then
            say 'Plan: fetch yay source, display PKGBUILD/.SRCINFO, then build as the login user.' \
                '計畫：下載 yay 原始碼，顯示 PKGBUILD 與 .SRCINFO，再以一般使用者建置。'
        else
            run git -c maintenance.auto=false clone --depth 1 https://aur.archlinux.org/yay.git "$work/yay"
            cat -- "$work/yay/PKGBUILD" "$work/yay/.SRCINFO"
        fi
        yes_no 'After reviewing the build recipe, build and install yay?' '閱讀建置腳本後，是否建置並安裝 yay？' || exit 0
        if $dry_run; then
            printf '+ makepkg --syncdeps --install (yay; unprivileged build)\n'
        else
            (cd -- "$work/yay" && run makepkg --syncdeps --install)
        fi
    fi
}
choose_apps() {
    stage=applications
    say 'Step 2/5 · Optional everyday applications' '步驟 2/5 · 選擇常用軟體'
    selected=()
    for _ in "${app_packages[@]}"; do selected+=(0); done
    yes_no 'Install any optional applications?' '是否安裝常用軟體？' || return 0
    for i in "${!selected[@]}"; do selected[$i]=1; done
    while true; do
        for i in "${!app_packages[@]}"; do
            local mark=' ' status=${app_sources[$i]}
            [[ ${selected[$i]} == 1 ]] && mark=x
            if app_installed "$i"; then
                status=$(say 'installed' '已安裝')
            fi
            printf ' %2d [%s] %-22s [%s]\n' "$((i + 1))" "$mark" "${app_names[$i]}" "$status"
        done
        ask 'all = select all, none = clear, numbers toggle; -N excludes. Example: all -3 -5. done = continue.' \
            'all 全選、none 清空、數字切換、-數字排除。例如 all -3 -5；done 繼續。'
        [[ $answer == done ]] && break
        local -a tokens=()
        read -ra tokens <<< "$answer"
        local valid=true token
        for token in "${tokens[@]}"; do
            if [[ $token != all && $token != none && ! $token =~ ^-?([1-9]|1[0-4])$ ]]; then valid=false; fi
        done
        if ! $valid || ((${#tokens[@]} == 0)); then
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
    say "Repository packages: ${official[*]:--}" "官方套件：${official[*]:--}"
    say "AUR packages: ${aur[*]:--}" "AUR 套件：${aur[*]:--}"
    yes_no 'Install this selection? Existing packages will not be removed.' '安裝以上選項？不會移除未選取或已安裝的軟體。' || return 0
    if ((${#official[@]})); then as_root pacman -S --needed "${official[@]}"; fi
    if ((${#aur[@]})); then run yay -S --needed "${aur[@]}"; fi
}
download_source() {
    stage=desktop-source
    say 'Step 3/5 · LunaDash desktop' '步驟 3/5 · 安裝 LunaDash 桌面環境'
    if $dry_run; then
        printf '+ Resolve %s@%s, download SHA-pinned HTTPS source archive, validate archive paths\n' "$repository" "$source_ref"
        printf '+ make-source.sh; makepkg --syncdeps --force; sudo pacman -U (built package)\n'
        return
    fi
    local revision
    revision=$(curl --proto '=https' --tlsv1.2 -fsSL --connect-timeout 10 --max-time 120 --retry 2 \
        "https://api.github.com/repos/$repository/commits/$source_ref" | jq -er '.sha')
    [[ $revision =~ ^[0-9a-f]{40}$ ]] || die 'Invalid GitHub source revision.' 'GitHub 回傳的版本識別碼無效。'
    printf 'Source revision: %s\n' "$revision"
    run curl --proto '=https' --tlsv1.2 -fsSL --connect-timeout 10 --max-time 600 --retry 2 \
        "https://codeload.github.com/$repository/tar.gz/$revision" -o "$work/source.tar.gz"
    python3 - "$work/source.tar.gz" "$work" "$revision" <<'PY'
import pathlib, sys, tarfile
archive, destination, revision = sys.argv[1:]
expected = 'LunaDash-' + revision
with tarfile.open(archive, 'r:gz') as source:
    members = source.getmembers()
    if len(members) > 20000 or sum(m.size for m in members) > 512 * 1024 * 1024:
        raise SystemExit('Source archive exceeds safe size limits')
    for entry in members:
        parts = pathlib.PurePosixPath(entry.name).parts
        if not parts or parts[0] != expected or '..' in parts or not (entry.isfile() or entry.isdir()):
            raise SystemExit('Unexpected source archive entry: ' + entry.name)
    source.extractall(destination, filter='data')
path = pathlib.Path(destination) / expected
(path / '.lunadash-revision').write_text(revision + '\n')
PY
    source_dir="$work/LunaDash-$revision"
    run bash "$source_dir/scripts/make-source.sh"
    stage=desktop-build
    (
        cd -- "$source_dir/packaging/arch"
        run makepkg --syncdeps --force
        local -a packages=()
        mapfile -t packages < <(makepkg --packagelist)
        ((${#packages[@]})) || exit 1
        for package in "${packages[@]}"; do [[ -f $package ]] || exit 1; done
        as_root pacman -U --needed "${packages[@]}"
    )
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
    local -a names=("Keep current / 保留目前設定" "Taiwan / 繁體中文" "United States / English"
        "China / 简体中文" "Japan / 日本語" "Korea / 한국어" "Germany / Deutsch"
        "France / Français" "Spain / Español" "Brazil / Português")
    for i in "${!locales[@]}"; do printf ' %2d) %s\n' "$i" "${names[$i]}"; done
    say ' 10) Other UTF-8 locale from /usr/share/i18n/SUPPORTED' ' 10) 其他 UTF-8 語系（參考 /usr/share/i18n/SUPPORTED）'
    local locale_code=
    while true; do
        ask 'Choose system language [0-10; default 0]:' '選擇系統語言 [0-10，預設 0]：'
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
    say "System locale: $locale_code" "系統語系：$locale_code"
    say "Fonts and input methods: ${fonts[*]}" "字型與輸入法：${fonts[*]}"
    if [[ $locale_code != keep ]]; then
        yes_no 'Generate and use this system locale? Keyboard layout and timezone stay unchanged.' \
            '產生並使用此系統語系？鍵盤配置與時區保持不變。' || locale_code=keep
    fi
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
        run lunadash-shell-tool language "$shell_locale"
    fi
    run fc-cache -f
}
configure_login() {
    stage=login
    say 'Step 5/5 · Wallpaper library and login' '步驟 5/5 · 桌布庫與登入設定'
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
    elif yes_no 'Install and enable SDDM for the next boot? The current session will not be stopped.' \
                 '安裝並啟用 SDDM，於下次開機使用？不會停止目前的工作階段。'; then
        as_root pacman -S --needed sddm
        as_root systemctl enable sddm.service
        as_root systemctl set-default graphical.target
    fi
    if ! command -v systemctl >/dev/null 2>&1 || ! systemctl is-enabled NetworkManager.service >/dev/null 2>&1; then
        if command -v systemctl >/dev/null 2>&1 && { systemctl is-enabled systemd-networkd.service >/dev/null 2>&1 || systemctl is-enabled connman.service >/dev/null 2>&1 || systemctl is-enabled iwd.service >/dev/null 2>&1; }; then
            say 'Another network service is configured; its configuration is unchanged.' '已設定其他網路服務，保持其設定不變。'
        elif yes_no 'Enable NetworkManager at the next boot (without changing the live connection)?' \
                    '下次開機啟用 NetworkManager（不改變目前連線）？'; then
            as_root systemctl enable NetworkManager.service
        fi
    fi
    if $dry_run; then
        say 'Preview complete; nothing was installed. Super+W will open the wallpaper gallery after installation.' \
            '預覽完成，未安裝任何內容。安裝後可按 Super+W 開啟桌布選單。'
    else
        say 'Installation steps completed. Choose LunaDash in your login manager. Super+W opens the wallpaper gallery.' \
            '安裝步驟已完成。請在登入管理器選擇 LunaDash；Super+W 可開啟桌布選單。'
    fi
    [[ -z $log_file ]] || printf 'Log: %s\n' "$log_file"
    if yes_no 'Reboot now? Save your work first.' '現在重新開機？請先儲存工作。'; then
        as_root systemctl reboot
    else
        say 'No reboot requested. Your current session is unchanged.' '未重新開機，目前工作階段保持不變。'
    fi
}
main() {
    while (($#)); do
        case "$1" in
            --help|-h) help; return ;;
            --dry-run) dry_run=true ;;
            --language)
                (($# >= 2)) || { help >&2; return 2; }
                requested_language=$2; shift
                [[ $requested_language == en || $requested_language == zh_TW ]] || return 2 ;;
            --ref)
                (($# >= 2)) || return 2
                source_ref=$2; shift
                [[ $source_ref =~ ^[A-Za-z0-9][A-Za-z0-9._/-]*$ && $source_ref != *..* ]] || return 2 ;;
            *) help >&2; return 2 ;;
        esac
        shift
    done
    # Open this before any prompt. No fallback to stdin: it may be the curl pipe.
    if ! { exec 3</dev/tty; } 2>/dev/null; then
        printf 'A terminal is required / 需要互動終端。Download the script and run it in a terminal.\n' >&2
        return 1
    fi
    trap finish EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
    banner
    choose_language
    preflight
    bootstrap
    choose_apps
    download_source
    choose_locale
    configure_login
}
# Keep execution after the complete function definitions for curl-pipe use.
main "$@"
