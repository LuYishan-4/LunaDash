# 開機登入、安裝與救援

[English](../en/LOGIN_SESSION.md) · [繁中索引](README.md)

LunaDash 仍是開發預覽版。安裝器會註冊真正的 Wayland login session，但巢狀測試不能證明每一種 DRM/KMS GPU、seat、VT、輸入裝置或 display manager 都能正常工作。測試期間請保留原本桌面或可登入的 TTY。

## 支援的安裝路徑

在 repo 根目錄以**一般使用者**執行：

```sh
./scripts/install-session.sh --dry-run
./scripts/install-session.sh
```

目前自動安裝路徑：

| 發行版家族 | 方式 |
| --- | --- |
| Arch / 衍生版 | `makepkg --syncdeps --force --install`，由 pacman 管理安裝檔 |
| Debian / Ubuntu | apt 安裝相依 → Ninja → `cmake --install /usr` |
| Fedora | dnf → Ninja → CMake install |
| openSUSE | zypper → Ninja → CMake install |
| Alpine | apk → Ninja → CMake install |
| Void | xbps → Ninja → CMake install |
| Gentoo | emerge → Ninja → CMake install |
| 其他 Linux | 驗證既有 toolchain 後走標準 CMake install |

只安裝／檢查相依：

```sh
./scripts/install-dependencies.sh --dry-run
./scripts/install-dependencies.sh
```

腳本不會新增 PPA/COPR/OBS 等第三方 repository。Quickshell 需 0.3+；若發行版沒有套件，請依上游安裝。已準備好相依時可用：

```sh
./scripts/install-session.sh --skip-deps
```

除了 Arch package path，其餘目前屬直接 CMake system install。腳本不會以 root 執行桌面、不會自動重啟電腦、不改預設 shell，也不會替你更換網路服務；只有 package/system install 階段才要求 sudo/doas/pkexec。

## 首次安裝引導與個人設定

從終端機進行全新安裝時，安裝器會先開啟引導，依序說明 LunaDash 主體、可選的 Arch 軟體群組、可編輯的頂部面板配置，以及參考作者設定製作的終端機範本，最後列出選項再確認。它會透過已安裝的 `lunadash-compositor`／`ludash-compositor` 或完成紀錄判斷是否已安裝。`--guided` 可重新開啟引導，`--skip-guide` 保留直接安裝流程。一般 `--dry-run` 與背景更新不會開啟引導；`--non-interactive` 不接受首次設定選項。

```sh
./scripts/install-session.sh --guided
# 也可直接指定選項；仍會保留套件管理器確認與系統授權。
./scripts/install-session.sh --skip-guide --apps basics,desktop,input \
  --desktop-profile --author-config
```

可選群組包含 `basics`（XDG、壓縮、傳輸工具及金鑰圈）、`desktop`（Chromium、Ark 與 Dolphin 的 KIO extras）、`media`（MPV、看圖與縮圖）、`office`（LibreOffice 與繁中語言包）、`development`（Fish、Starship、Fastfetch、搜尋及終端機工具）和 `input`（Fcitx5 中文輸入法、CJK 與 JetBrains Mono 字型）。`--apps none` 不加裝選用群組。除非使用 `--skip-deps`，主體相依套件仍會安裝。選用軟體群組目前以 Arch 已設定的 pacman 來源為目標；其他發行版保留原本主體安裝流程，也能使用配置範本，額外程式則由使用者安裝對應套件。安裝器不會安裝 AUR helper 或新增套件來源。

配置組織與可攜外觀參考作者提供的 2026-09-24 設定快照及 [NyxNiri](https://github.com/ech678/NyxNiri)。LunaDash 沿用自己的 compositor、Quickshell UI 與設定 schema；不匯入參考包的 Niri／Noctalia 工作階段、個人身分、系統設定、硬體規則或瀏覽器狀態。Kitty 範本採用參考字型、留白、透明度及複製貼上快捷鍵；Fish、Starship 和 Fastfetch 提供可編輯初始設定，不變更帳號預設 shell。安裝時不會執行作者設定包內的還原程式。

`--desktop-profile` 只在檔案不存在時建立 `${XDG_CONFIG_HOME:-$HOME/.config}/LuDash/shell-modules.json`。預設浮動頂部面板包含精簡工作區、目前應用程式、CPU／記憶體、置中啟動器與右側控制項。底部 Dock 預設關閉，可在 Settings 重新開啟。面板可透過 Settings 或既有 module JSON 調整，未指定欄位沿用內建範本；現有桌面配置與已儲存偏好仍優先。

`--author-config` 只建立整組尚不存在的應用程式設定。既有檔案及符號連結都會保留，範例另放在 `${XDG_CONFIG_HOME:-$HOME/.config}/LuDash/setup-examples/` 供比較；手動合併前請先備份原設定。Kitty 最後載入 `__custom__.conf`，Fish 最後載入 `__custom__.fish`；Starship 與 Fastfetch 使用各自可直接編輯的設定檔。寫入前會拒絕父目錄為符號連結的路徑，並支援含空白的絕對 XDG 路徑。首次完成紀錄與新增檔案雜湊會保存在 `${XDG_STATE_HOME:-$HOME/.local/state}/lunadash/setup/complete.json`；這是紀錄而非自動回復工具。完整範本與群組來源見[首次安裝範本說明](../../data/setup/README.md)。

## Display manager / SDDM

如果已經有登入管理器，保留它即可。登出前：

```sh
lunadash-session --check
```

確認 `/usr/share/wayland-sessions/lunadash.desktop` 存在，登出後在工作階段選單選 **LunaDash (Wayland)**。

沒有 display manager、而且是 systemd 系統時：

```sh
./scripts/install-session.sh --enable-sddm
```

這只會安裝/enable SDDM 並把下一次開機 target 設為 graphical；**不會立刻切換目前桌面**。若偵測到另一個已啟用的 display manager 會拒絕取代。OpenRC/runit 請依各發行版方式自行 enable。

確認一般登入/登出正常後，才考慮自動登入：

```sh
./scripts/install-session.sh --enable-sddm --autologin "$(id -un)"
```

它只建立 `/etc/sddm.conf.d/90-ludash-autologin.conf`，而且不覆寫既有檔案。共用電腦不建議啟用。

## 工作階段啟動

`/usr/share/wayland-sessions/lunadash.desktop` 會執行 `/usr/bin/lunadash-session`。它建立 private D-Bus session，再啟動 wlroots DRM/KMS compositor。Wayland socket 建好後，才準備 Quickshell、可選 Fcitx5 與應用程式環境，並把需要的變數發布到該 session 的 D-Bus／systemd user environment。

巢狀 compositor 不會覆寫 host desktop 的 activation environment。Session log 位於：

```text
${XDG_STATE_HOME:-$HOME/.local/state}/lunadash/session-*.log
```

## 在既有 Wayland 桌面巢狀測試

```sh
env -u MESA_GL_VERSION_OVERRIDE -u MESA_GLSL_VERSION_OVERRIDE \
  QT_QPA_PLATFORM=wayland \
  lunadash-compositor --nested --socket ludash-test
```

source build 則改用 `./build/lunadash-compositor`。不要把 `lunadash-session` 當巢狀啟動器，它是登入 session entry。

## 首次登入

第一次會看到 welcome 與網站 help。按 Start desktop 後，用 Settings 選語言、外觀與網路。既有 NetworkManager 連線會直接重用。Suspend/reboot/poweroff 走 logind D-Bus，不把密碼放進 LunaDash。

## 發行版注意事項

Arch 是主要開發環境，安裝後可用：

```sh
sudo pacman -R ludash
```

Debian/Ubuntu、Fedora、openSUSE、Alpine 由 dependency helper 選對應 Qt 6、Wayland、wlroots/input/GL 等套件再走 CMake install。Void/Gentoo 也有安裝路徑，但 CI breadth 比前述平台小。其他 Linux 可先依 [測試與建置](TESTING_AND_FILES.md) 準備 toolchain，再：

```sh
./scripts/install-session.sh --skip-deps
```

或手動：

```sh
cmake -S . -B build-login -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-login --parallel
sudo cmake --install build-login
```

## XWayland、Discord 與原生 Wayland

LunaDash 仍是 Wayland compositor。XWayland 是**按需**相容層。Discord 1.0.157 的 input helper 即使主視窗跑 native Wayland，也可能需要 X display；launcher 會先準備 LunaDash 自己的 authenticated XWayland，提供 `DISPLAY` / `XAUTHORITY`，但讓主視窗保留 `WAYLAND_DISPLAY`。

一般 native Wayland 程式如果也需要 X11 helper，可用：

```sh
lunadashctl launch-with-x11 -- application --argument
```

這會保留 Wayland 環境並讓新的 X11 root window 保持隱藏。完整限制見 [XWayland](XWAYLAND.md)。

## 更新與 polkit

About 頁的背景更新會自動接受 package transaction，但**不繞過系統授權**。到安裝階段會顯示 Waiting for authorization，並讓出 keyboard focus 給系統 polkit dialog。沒有 graphical polkit agent 時會明確失敗，不會在背景卡著隱藏的 terminal prompt。

LunaDash session 會嘗試啟動發行版提供的 polkit agent。Arch package 包含 `polkit-kde-agent`；其他發行版需安裝 KDE/LXQt/GNOME 等 agent。更新完成後請登出再登入或 reboot，因為 package replacement 不會讓現有 QML process 自動完整 reload。

## 救援

一般情況只要登出並選回舊桌面。若 auto-login 讓你到不了 greeter，切到 TTY 後：

```sh
sudo rm -- /etc/sddm.conf.d/90-ludash-autologin.conf
```

需要恢復純文字開機（systemd）可用 `sudo systemctl set-default multi-user.target`；這只影響下次啟動。

若 LunaDash 登入立刻退出，查看 session log 與：

```sh
journalctl -b -u sddm.service
```

不要用 root 啟動桌面，也不要把 GPU/input device node 改成所有人可寫。

舊 `ludash-compositor`、`ludash-desktop`、`ludashctl`、`ludash-session` 與 `ludash.desktop` 仍保留相容 alias；新整合請使用 `lunadash-*` 名稱。

Dolphin 是主體執行相依套件，即使未選擇任何選用軟體群組仍會安裝，取代已移除的內建檔案管理器。
