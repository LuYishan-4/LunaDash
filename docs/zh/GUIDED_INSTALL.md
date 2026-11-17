# 引導安裝與桌面操作

## 使用 curl 安裝

適用於**已安裝的 Arch Linux x86_64**，需要網路、Bash、curl，以及具備 sudo
權限的一般帳號。這不是磁碟分割或空白硬碟安裝程式。請先檢查下載的程式碼；
LunaDash 1.0.2a 仍是開發中的桌面環境。

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash
```

不需要先 git clone。透過管線執行的腳本是一個引導程式：缺少 Git 時先安裝 Git，
把 LunaDash 原始碼下載到 `~/.cache/lunadash/source`（有設定 `$XDG_CACHE_HOME`
時改用它），在 `~/.local/bin` 建立 `lunadash` 指令的連結，再交給該副本裡的
`install.sh` 繼續安裝。分支（預設 `dev`，或以 `--ref` 指定）會先在 GitHub 解析成
確切的提交，下載的內容必須與該提交相符。

連不上 GitHub 時，改經由 `https://gh-proxy.org/` 鏡像下載。LunaDash-Shell 與
SlintLu 一律依 `packaging/*-revision` 固定的提交下載，鏡像無法替換內容。至於
LunaDash 分支本身，只有在 GitHub 完全沒有回應時才採用鏡像提供的提交編號，
安裝程式會明確提示。`LUNADASH_REPO=URL` 只使用指定的儲存庫（fork 或本地鏡像）；
`LUNADASH_GIT_MIRRORS` 可替換固定元件使用的鏡像清單（以空白分隔的 URL 前綴，
留空表示只用 GitHub）。

問題與子程序的互動輸入都從 `/dev/tty` 讀取，不會把承載腳本的管線當成回答。
安裝程式拒絕以 root 執行，也不以 root 建置 AUR。若缺少 sudo，可經由 su 的 root
驗證安裝，但不會授予 sudo 權限或修改 sudoers。

也可以先下載、閱讀，再預覽不寫入系統的安裝計畫：

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh -o install.sh
less install.sh
bash install.sh --dry-run
```

安裝程式的語言依 `LANG` 決定：任何中文語系都顯示繁體中文，其他顯示英文；
`--language en|zh_TW` 可以覆寫。它不會改變系統語系、鍵盤或時區。後面的獨立步驟
才選擇系統 UTF-8 語系，也可保留原設定。中、日、韓會補上 CJK 字型與對應 Fcitx
輸入法；其他文字系統補上額外 Noto 字型。

### 安裝之後

```bash
lunadash update     # 更新到已安裝分支的最新原始碼、更新系統並重新建置
lunadash install    # 重新執行安裝導覽
lunadash status     # 顯示快取原始碼、分支、提交與已安裝的套件
```

`lunadash update` 不會詢問安裝問題：它抓取安裝時記錄的分支（或 `--ref`），
執行 `pacman -Syu`（pacman 仍會要求確認），再重新建置並安裝套件。若
`~/.local/bin/lunadash` 已經是其他檔案，會保留不動。從其他 LunaDash 副本執行
`install.sh` 時，會直接安裝該副本，不下載也不建立指令連結。

### 五個步驟

安裝程式只在需要選擇時詢問；pacman 與 yay 變更系統前仍會列出套件並要求確認。

1. 安裝基本工具並同時執行完整 `pacman -Syu`（不做不受支援的局部更新）：超過
   20 個工具，包含 glibc、Git、curl、憑證、壓縮工具、編輯器、NetworkManager／nmcli、
   韌體、開發工具、字型與 D-Bus。
2. 常用軟體選單：Chrome、Discord、Zed、Microsoft Visual Studio Code、OBS、Spotify、
   ProtonPlus、Kitty、Fish、Dolphin、FileZilla、htop、Nmap、Tor。預設全不選；
   輸入數字切換，支援 `all`、`none` 與 `all -3 -5` 等排除方式，直接按 Enter 繼續。
   已安裝的原生或已知 Flatpak 軟體會標記並略過；未選取的軟體不會被移除。官方
   套件使用 pacman，AUR 使用 yay。只有選了 AUR 軟體且缺少 yay 時，才會下載並顯示
   yay 的官方 AUR 建置配方，確認一次後以一般使用者建置。安裝 Tor 不代表啟用其服務。
3. 以一般使用者從快取原始碼建置套件，LunaDash 殼層與 SlintLu 使用固定版本，僅安裝
   套件時提權。工作階段預設啟動 LunaDash 殼層；`--shell custom --shell-command waybar`
   或 `--shell none` 會寫入 `~/.config/LunaDash/session.json`
   （見[桌面殼層](SHELL.md#選擇殼層)）；沒有指定 `--shell` 時保留既有的選擇。
4. 選擇系統語言（直接按 Enter 保留目前設定），安裝必要字型與輸入法。改動
   `locale.gen` 前建立備份；不根據國家猜測鍵盤配置或時區。尚無 LunaDash 介面翻譯的
   語言以英文顯示，不影響您選擇的系統語系。
5. 建立尚不存在的 Pictures/Wallpapers 桌布資料夾。保留既有登入管理器；沒有登入管理器
   時詢問一次是否啟用 SDDM，於下次開機使用，不中斷目前的桌面。沒有設定其他網路服務
   （systemd-networkd、ConnMan、iwd、dhcpcd）時，自動在下次開機啟用 NetworkManager，
   不更動目前的連線。安裝程式不會自行重開機，只會提示要登出還是重開機。

任何步驟失敗都會停止。私人日誌存於 `$XDG_STATE_HOME/lunadash` 或
`~/.local/state/lunadash`，權限 0600；暫存建置檔會清除，原始碼快取保留給
`lunadash update` 使用。已安裝的系統套件不會被擅自回退，回報問題時請保留日誌。

`install.sh` 是引導式入口；**`scripts/install-session.sh` 保持原樣**，仍供既有開發與
測試流程使用。其他發行版維持原始碼／依賴安裝流程；此引導式常用軟體清單不宣稱跨
發行版通用。

## 桌布選單

**Super+W** 直接開啟／收回可模板化的縮圖選單，不再先開設定或任意路徑選擇器。
預設資料夾是 Pictures/Wallpapers，可按資料夾按鈕修改路徑。將圖片或影片放入
後，最多約五秒會重新探索；重新整理按鈕可立即重掃。一般探索上限為 512 張，
另保留最近使用與收藏項目，支援一層分類資料夾，不遞迴追蹤資料夾符號連結。

選單提供搜尋、分類、靜態／動態篩選、桌布庫／內建／收藏、深色／淺色／自動，
點選即切換。開啟後可直接輸入搜尋；Enter 套用、方向鍵移動，Escape 先清空
搜尋再關閉，再按快捷鍵或點擊空白處也能收回。收藏不會因切換圖庫消失。
影片使用預覽圖，不會把 MP4 交給 `Image` 解碼；檔案路徑當資料傳遞，不插入
shell 指令。模板擴充目標為 `wallpaper-gallery`，沿用 SDK 2。

## 控制中心與設定

控制中心的顯示器、網路、藍牙、電源、外觀與系統入口，會**在控制中心內直接
顯示可調整頁面**。只有右上齒輪進入完整設定；返回／Escape 可回主頁。
需要權限的系統工具保留原本授權流程，不在桌面內儲存密碼。

設定頁開啟／關閉沿用共用動畫模板，放大／還原會插值至有效工作區範圍。
About 顯示版本、提交、硬體數據、更新狀態與原始碼／社群入口。GPU 計數器
無法使用時明確顯示無資料，不假裝是零負載。

設定、控制中心與桌布選單透過 compositor 的 `WindowGlass` 擷取真正的下層
畫面並模糊，不模糊面板自己的文字。背景尚未就緒、關閉模糊、渲染失敗或啟用
護眼模式時，改用可讀的不透明底色。動畫尊重減少動態效果設定，不依賴 KWin
或 KDE 專屬模糊 API。

## 驗證範圍

測試包含 curl 管線與 PTY 輸入分離、語言獨立性、套件選取、有無 Git 的原始碼
封裝、桌布探索／收藏、邊界模型、毛玻璃降級、原生背景生命週期及 Quickshell
實際畫面。無頭 CI 不等於已驗證專有 GPU 驅動、真實網路變更、全新電腦完整
安裝或實體重開機；這些仍須實機驗收。
