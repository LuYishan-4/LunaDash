# 引導安裝與桌面操作

## 使用 curl 安裝

適用於**已安裝的 Arch Linux x86_64**，需要網路、Bash、curl，以及具備 sudo
權限的一般帳號。這不是磁碟分割或空白硬碟安裝程式。請先檢查下載的程式碼；
LunaDash 1.0.1a 仍是開發中的桌面環境。

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh | bash
```

第一次執行不需要先 git clone。問題與子程序的互動輸入都從 `/dev/tty` 讀取，
不會把承載腳本的管線當成回答。正式安裝拒絕 root／sudo bash，不以 root 建置
AUR。若缺少 sudo，可經由 su 的 root 驗證安裝，但不會擅自修改 sudoers；沒有
授權的帳號仍須先取得系統管理員授權。

也可以先下載、閱讀，再預覽不寫入系統的安裝計畫：

```bash
curl -fsSL --connect-timeout 10 https://raw.githubusercontent.com/LuYishan-4/LunaDash/dev/install.sh -o install.sh
less install.sh
bash install.sh --dry-run
```

起始的繁體中文／英文選擇**只控制安裝程式的語言**，不會改變系統語系、鍵盤
或時區。套件管理器的輸出仍依系統語系顯示。後面的獨立步驟才選擇系統 UTF-8
語系，也可保留原設定。中、日、韓會補上 CJK 字型與對應 Fcitx 輸入法；其他
文字系統補上額外 Noto 字型。純 Linux 虛擬主控台仍需要能顯示該文字的終端。

### 五個步驟

1. 顯示並安裝超過 20 個基本工具，包含 glibc、Git、curl、憑證、壓縮工具、
   編輯器、NetworkManager／nmcli、韌體、開發工具、字型與 D-Bus。使用完整
   `pacman -Syu`，不進行不受支援的局部更新。若沒有 yay，會下載官方 AUR
   原始碼、顯示建置配方，並在再次確認後以一般使用者建置。
2. 詢問是否安裝常用軟體：Chrome、Discord、Zed、Microsoft Visual Studio Code、
   OBS、Spotify、ProtonPlus、Kitty、Fish、Dolphin、FileZilla、htop、Nmap、Tor。
   支援 `all` 全選、`none` 清空、數字切換，以及 `all -3 -5` 等排除方式，最後
   輸入 `done`。已安裝的原生或已知 Flatpak 軟體會標記並略過；未選取的軟體
   不會被移除。官方套件使用 pacman，AUR 使用 yay 並保留正常審查與確認。
   安裝 Tor 不代表自動啟用其服務。
3. 將 LunaDash 分支解析成確切提交 SHA，透過 HTTPS 下載該版本，檢查封存檔
   路徑及大小，並以一般使用者建置套件，僅安裝套件時提權。沒有 `.git` 也能
   打包；`.lunadash-revision` 記錄下載版本。`--ref SHA` 可固定桌面原始碼版本，
   但這不代表對最初下載的安裝腳本另做獨立簽章驗證。
4. 獨立選擇系統語言，安裝必要字型與輸入法。改動 `locale.gen` 前建立備份；
   不根據國家猜測鍵盤配置或時區。尚無 LunaDash 介面翻譯的語言以英文顯示，
   不影響您選擇的系統語系。
5. 建立尚不存在的 Pictures/Wallpapers 桌布資料夾。保留既有登入管理器；可選擇
   啟用 SDDM，於下次開機使用，不立即中斷桌面。沒有衝突的網路管理器時，才
   在確認後啟用 NetworkManager。全部完成後詢問是否重開機，預設**不重開**。

任何步驟失敗都會停止，不會繼續到重開機。私人日誌存於
`$XDG_STATE_HOME/lunadash` 或 `~/.local/state/lunadash`，權限 0600；暫存原始碼與
建置檔會清除。已安裝的系統套件不會被擅自回退，回報問題時請保留日誌。

新的 `install.sh` 是引導式入口；**`scripts/install-session.sh` 保持原樣**，
仍供既有開發與測試流程使用。其他發行版維持原始碼／依賴安裝流程；此引導式
常用軟體清單不宣稱跨發行版通用。

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
