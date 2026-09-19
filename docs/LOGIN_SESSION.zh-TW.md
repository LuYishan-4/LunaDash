# LunaDash 開機登入與安裝指南

這份文件說明如何安裝 LunaDash，讓電腦開機後能從登入畫面進入 LunaDash，以及如何選擇自動登入。

**目前 LunaDash 仍是開發預覽版。** 已有巢狀 Wayland 測試結果，但實體 GPU 的 wlroots DRM/KMS 啟動、輸入裝置權限與 VT 切換尚未完成驗證。鎖定畫面、多螢幕、完整桌面 portal 與 polkit 驗證代理也尚未完成。測試期間請保留原本可使用的桌面。

## 1. 先選擇你的使用方式

| 需求 | 使用方式 |
| --- | --- |
| 在目前桌面裡先試用 LunaDash | 執行巢狀 Wayland 工作階段 |
| 開機後輸入密碼，再進入 LunaDash | 安裝套件，在登入畫面選擇 LunaDash |
| 沒有登入管理器，希望開機出現登入畫面 | 安裝時加上 `--enable-sddm` |
| 開機直接進入 LunaDash，不輸入密碼 | 另外明確指定 `--autologin` |

安裝腳本目前以 Arch Linux／pacman 系統為目標。其他發行版的手動安裝方式在本文最後。

## 2. 在 Arch Linux 安裝

在專案根目錄操作，使用一般使用者帳號。**不要在整條腳本前面加上 `sudo`**；需要權限時，腳本中的 sudo／pacman 會自行詢問。

先預覽會執行的指令：

```sh
./scripts/install-session.sh --dry-run
```

確認後安裝。安裝完成後，先執行 `lunadash-session --check`，並確認 `/usr/share/wayland-sessions/lunadash.desktop` 與 `/usr/share/icons/hicolor/512x512/apps/lunadash.png` 已存在：

```sh
./scripts/install-session.sh
```

腳本會依序安裝建置工具、產生原始碼封裝，再透過 `makepkg --syncdeps --force --install` 編譯並安裝。安裝檔案由 pacman 管理，之後可以使用 pacman 移除。

如果已經有登入管理器，安裝完成後儲存工作並登出，在登入畫面的工作階段選單選擇 **LunaDash (Wayland)**，再登入即可。SDDM 預設會記住上次選擇的工作階段；若你的系統改過相關設定，請以實際設定為準。

### 沒有登入管理器時

使用這個選項安裝並啟用 SDDM：

```sh
./scripts/install-session.sh --enable-sddm
```

它會啟用 `sddm.service`，並將下次開機的預設目標設為 `graphical.target`。腳本不會立即啟動或重啟登入管理器，也不會自動登出或重新開機；請儲存工作後自行重開機。實際測試時，依序執行 `./scripts/install-session.sh --dry-run` 與 `./scripts/install-session.sh --enable-sddm`，用 `systemctl is-enabled sddm.service` 確認啟用狀態，但不要在目前桌面中立即啟動 SDDM。自行重新開機後，先手動選擇 **LunaDash (Wayland)** 登入及登出；確認成功後才考慮自動登入。若登入畫面立即返回，請保留工作階段日誌並改從原桌面或 TTY 排查。

如果系統已啟用其他登入管理器，這個選項會停止並提示你保留現有管理器。此時改用不帶 `--enable-sddm` 的安裝指令即可。

SDDM 的登入畫面與 LunaDash 工作階段各自有顯示後端；腳本會保留 SDDM 原有的登入畫面設定，LunaDash 本身使用 Wayland。

## 3. 開機自動進入 LunaDash

先確認手動登入成功，再使用目前帳號啟用自動登入：

```sh
./scripts/install-session.sh --enable-sddm --autologin "$(id -un)"
```

**這會略過開機時的密碼登入。** 需要登入保護的共用電腦，請保留一般登入方式。

腳本會建立：

```text
/etc/sddm.conf.d/90-ludash-autologin.conf
```

內容如下，其中 `your-user` 會換成你指定的既有帳號：

```ini
[Autologin]
User=your-user
Session=lunadash.desktop
Relogin=false
```

`Relogin=false` 表示登出後不會立刻再次自動登入。若該設定檔已存在，腳本會拒絕覆寫；請先手動檢查。`/etc/sddm.conf` 或其他設定檔中的 `[Autologin]` 也可能覆蓋這份設定。

## 4. 安裝完成後如何確認

先檢查必要指令與登入執行期目錄：

```sh
lunadash-session --check
```

安裝後的 `lunadash-session` 刻意不使用 `.sh` 副檔名，與原始碼中的 `scripts/install-session.sh`、`scripts/test-wayland.sh` 等輔助腳本不同。這只檢查基本啟動條件，**不會取得顯示卡控制權，也不代表實體登入測試已通過**。

Arch 套件會安裝這些入口：

| 檔案 | 用途 |
| --- | --- |
| `/usr/share/wayland-sessions/lunadash.desktop` | 讓登入管理器列出 LunaDash 工作階段 |
| `/usr/bin/lunadash-session` | 檢查環境、建立日誌，透過 wlroots DRM/KMS 與 D-Bus 啟動桌面 |
| `/usr/bin/lunadash-compositor` | 管理 Wayland 視窗、平鋪與合成繪製 |
| `/usr/bin/lunadash-desktop` | 啟動 LunaDash 內建應用程式 |
| `/usr/bin/lunadashctl` | 向執行中的桌面傳送控制指令 |
| `/usr/share/applications/lunadash-app.desktop` | 一般應用程式的桌面識別碼 |
| `/usr/share/lunadash/shell/` | 安裝後的 Quickshell QML |

Wayland socket 建立後，合成器會準備只供 Quickshell、選用的 Fcitx5 與應用程式使用的環境，不會修改宿主桌面的 D-Bus 啟動環境或 systemd 使用者環境。獨立登入預設使用 OpenGL ES 3。需要桌面 OpenGL 時，可以在工作階段環境中設定 `LUDASH_GRAPHICS=opengl`；這要求 OpenGL 3.3 相容設定檔。不同登入管理器載入環境設定的方式不同，單純在另一個終端機執行 `export` 不一定會影響下次登入。

## 5. 先在現有 Wayland 桌面試用

不要在正在執行的桌面裡直接啟動 `lunadash-session`；它是獨立登入入口。巢狀試用請執行：

```sh
LUNADASH_DISABLE_FCITX=1 QT_QPA_PLATFORM=wayland lunadash-compositor --socket ludash-test
```

尚未安裝、但已完成本機編譯時：

```sh
LUNADASH_DISABLE_FCITX=1 QT_QPA_PLATFORM=wayland ./build/lunadash-compositor --socket ludash-test
```

這會在目前桌面內開啟 LunaDash 視窗，方便先確認介面、設定與應用程式。`LUNADASH_DISABLE_FCITX=1` 可避免巢狀測試啟動或取代宿主的 Fcitx；只有刻意測試 Fcitx／SNI 時才移除此變數。請確認左側面板可切換工作區並開啟工作階段選單；中央裁切顯示品牌圖示，按下後啟動器從面板中央向下展開；啟動器只有一份應用程式清單；右側顯示精簡狀態與可用的 StatusNotifier 圖示。Fcitx 圖示能顯示不代表候選字視窗或 input-method-v2 橋接已完成驗證。宿主桌面可能攔截部分 Super 快捷鍵，可改用 LunaDash 面板上的按鈕。

## 6. 第一次登入與自訂

首次啟動的設定精靈提供語言、網路與外觀設定，也允許離線繼續。LunaDash 會使用系統既有網路連線，安裝腳本不會替換網路服務。

之後可開啟設定，使用以設定項目為單位的斷詞搜尋，並查看擴充後的「關於」頁面；也可調整主題色、模糊、視窗間距、動畫、工作區與預設應用程式。工作階段選單支援登出，並透過 logind D-Bus 執行系統允許的暫停、重新開機與關機；破壞性操作會先確認並檢查可用性，不會執行 shell 指令。預設終端機為 Kitty，搭配 LunaDash 的 Fish 設定；內建檔案管理器會跟隨桌面色彩。

進階功能請參考以下英文文件：

- [設定功能與限制](SETTINGS.md)
- [模組 JSON、自訂 QML 與範本](MODULES.md)
- [預設終端機與檔案管理器](DEFAULT_APPS_AND_FILES.md)
- [所有維護中檔案的用途與測試方式](TESTING_AND_FILES.md)

## 7. 如何跑測試

以下指令在專案根目錄執行，假設 `build` 已完成編譯。

一般測試需要的 Arch 工具：

```sh
sudo pacman -S --needed xorg-server-xvfb xorg-xauth xdotool python python-pillow
```

執行軟體繪圖的 Wayland 整合測試：

```sh
LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

從正在執行的 Wayland 桌面測試宿主 GPU 路徑：

```sh
LUDASH_TEST_HOST_WAYLAND=1 LUDASH_GRAPHICS=opengl ./scripts/test-wayland.sh
LUDASH_TEST_HOST_WAYLAND=1 LUDASH_GRAPHICS=gles ./scripts/test-wayland.sh
```

測試首次設定、儲存偏好與重新啟動：

```sh
xvfb-run -a -s '-screen 0 1440x900x24' python3 tests/wayland/test_setup.py build
```

整合測試成功必須同時具備正常結束碼、有效的視窗內容與乾淨關閉結果。出現 shader／pipeline 錯誤不能視為通過。首次設定日誌保存在 `build/ci-evidence/setup/`；宿主 GPU 測試的截圖為 `build/host-wayland-preview.png`。

目前已完成本機一般與 ASan／UBSan 的 Wayland 整合測試，以及 GL／GLES、設定、自訂功能和 X11 相容等測試。GitHub Actions／CodeQL 的遠端結果仍須以實際 workflow 為準。上述結果不等同於已完成開機登入驗證。

## 8. 黑畫面或登入後立刻返回

登入日誌預設位於：

```text
~/.local/state/lunadash/session-*.log
```

若有設定 `XDG_STATE_HOME`，則位於該目錄下的 `lunadash/`。每次登入會建立新的私人日誌，舊日誌可自行清理。

檢查日誌中的 EGL、DRM、輸入裝置或權限錯誤，也可以查看 SDDM 日誌：

```sh
journalctl -b -u sddm.service
```

顯示卡與輸入裝置權限取決於 PAM/logind 工作階段及 Qt 後端。不要改用 root 執行桌面，也不要把裝置節點改成任何人皆可寫入。

先前 Qt 6.11.2 的 shader 問題已加入 OpenGL 相容設定檔與共用 context 修正；詳細原因見 [繪圖後端說明](GRAPHICS.md)。目前宿主 EGLStream 結束時仍可能出現 Qt 的 orphaned textures 警告，尚未解決；目前測試不宣稱已涵蓋資源洩漏。

## 9. 回到原本桌面或移除

一般情況下，登出後在登入畫面選回原本的桌面即可。

如果自動登入導致無法停在登入畫面，可切換到其他 TTY，登入後移除 LunaDash 的自動登入設定：

```sh
sudo rm -- /etc/sddm.conf.d/90-ludash-autologin.conf
```

儲存工作後自行重新開機。如果無法切換 VT，請使用發行版的救援開機方式處理。

要移除 LunaDash，先登入另一個桌面，再執行：

```sh
sudo pacman -R ludash
```

這不會刪除使用者偏好設定。若其他桌面仍使用 SDDM，就保留 SDDM。

若你希望下次開機只進入文字介面：

```sh
sudo systemctl set-default multi-user.target
```

這會變更下次開機目標，不會立即停止目前桌面。

## 10. 其他 Linux 發行版

安裝 [README](../README.md) 所列的編譯依賴，以及 Quickshell、D-Bus、Kitty、Fish 後，可以使用 CMake 手動安裝：

```sh
cmake -S . -B build-login -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build-login --parallel 4
sudo cmake --install build-login
```

這是直接從原始碼安裝，不會建立由 apt／dnf 管理的套件，也不能使用 `pacman -R` 移除。登入管理器請依照發行版設定；腳本不會自動調整 apt／dnf 系統的服務。

其他平台的實體登入工作階段同樣尚未驗證。英文版請見 [Boot and login session](LOGIN_SESSION.md)。

## NVIDIA 相容模式

新版偵測到 NVIDIA 驅動時，預設讓 Quickshell 使用軟體繪圖，合成器仍使用 OpenGL／GLES 和模糊效果。這是針對長時間執行後同步描述符累積、導致 `Too many open files` 的相容處理；可能增加 CPU 使用量，也不代表其他 GPU 應用程式的驅動問題已修復。更新後需要重新啟動 LunaDash 工作階段。

在現有 Wayland 桌面執行持續測試：

```sh
LUDASH_TEST_HOST_WAYLAND=1 python3 tests/wayland/test_resource_lifetime.py build
```

測試約需一分鐘，紀錄在 `build/ci-evidence/resource-host.log` 和 `.json`。需要手動指定時，在啟動指令前加上 `LUDASH_SHELL_RENDERER=software`；`opengl` 可強制使用 GPU，`auto` 為預設。詳細限制見 [英文說明](SHELL_RENDERING.md)。


舊有的 `ludash-compositor`、`ludash-desktop`、`ludashctl`、`ludash-session` 與 `ludash.desktop` 僅為相容別名；新的整合應使用上述 LunaDash 名稱。目前沒有宣稱已完成實體 SDDM 或 Fcitx 驗證。
