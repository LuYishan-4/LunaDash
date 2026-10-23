# Portal 執行流程與來源選擇

LunaDash 保留自製的檔案與螢幕來源選擇介面，不改成 Dolphin 或 KDE。
檔案選擇器的外框、手動路徑輸入、篩選器與共用桌面主題維持原設計。
螢幕分享仍由 xdg-desktop-portal-wlr 與 PipeWire 處理，LunaDash 的來源
選擇視窗本身不是影片串流後端。

## 登入時的路由

登入腳本優先沿用 DBUS_SESSION_BUS_ADDRESS。未設定地址但
$XDG_RUNTIME_DIR/bus 已存在時，改用 PAM 的使用者匯流排，避免另外
建立與 systemd 使用者服務隔離的私人 D-Bus。

已安裝的登入環境若尚無使用者指定的 LunaDash 設定，腳本會把安裝的預設
路由複製至 $XDG_CONFIG_HOME/xdg-desktop-portal/lunadash-portals.conf，
通常位於 ~/.config 下。這可避免其他桌面留下的通用 portals.conf 在
LunaDash 中選到不相容的後端。通用設定、已存在的 LunaDash 專用設定與
符號連結都會保留，不覆寫；--check 預先檢查不會建立設定檔。

預設 FileChooser 與 Settings 使用 lunadash，ScreenCast 與 Screenshot
使用 wlr。已有的使用者 LunaDash 專用覆寫仍可選用其他後端；更新後路由
沒有改變時，應先檢查該檔案。

## 後端介面

D-Bus backend 使用 GLib GDBus，先匯出 FileChooser 第 4 版與 Settings 第 2
版，再取得服務名稱。FileChooser 與 ScreenCast 各自啟動獨立的 Slint／Winit
Wayland client。FileChooser helper 從 stdin 接收 JSON，並在 stdout 傳回確認後
的本機路徑與 portal 選項。`Request.Close` 會結束目前的 picker 程序。啟動
成功與失敗會寫入 stderr／journal。

dmenu 模式必須原樣回傳選中的輸入行；UTF-8 名稱與尾端空白都是來源標籤
的一部分。刪除空白可能使 xdpw 把使用者已確認的來源判成未知來源。
取消時不輸出來源；縮圖擷取失敗不應阻止選擇。

## 自動執行的核心測試

PR 規則禁止修改 .github/workflows，因此回歸測試改由 CTest 註冊，
不新增 workflow。啟用 BUILD_TESTING 時，既有 Ubuntu 建置會執行
lunadash-portal-protocol、lunadash-session-launcher，以及原本的
lunadash-portal-picker。協議測試透過 Qt Test 與 dbus-run-session，
在私人匯流排啟動另一個真正的後端程序，檢查第一次查詢的函式／訊號型別、
FileChooser 第 4 版、Settings.Read，以及 OpenFile／SaveFile 取消後
後端是否仍存活。它不需要 Xvfb 或 Python D-Bus 套件。登入測試另檢查
匯流排重用、路由衝突與使用者設定保留。

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build --target lunadash-portal-protocol-test lunadash-portal-picker-test
ctest --test-dir build --output-on-failure -R '^lunadash-(portal-protocol|portal-picker|session-launcher)$'
```

## 完整前端與介面整合測試

tests/portal/test_runtime.py 另外驗證真正的開檔／存檔／SaveFiles
確認與回傳 URI、透過真正 xdg-desktop-portal 前端呼叫剛啟動的後端、
公開 Request.Response 與深淺色設定、沒有遞迴啟動前端，以及來源選擇器
的確認／取消與 UTF-8、空白完整保留。測試會啟動獨立的 headless wlroots
compositor，並透過 `wtype` 在 Wayland 上操作 Slint 選擇器，也會保留介面截圖；
不使用 AUTOPICK 捷徑。
主要 Ubuntu workflow 會啟用這組測試並透過 CTest 執行；其他 job 與本機建置
只有在明確開啟選項時才會執行。回報時應以實際執行該測試的 job 結果為準。

先安裝 xdg-desktop-portal、Xvfb、xauth、wtype 與所選 Python 的
dbus／gi 套件。Ubuntu 對應 xdg-desktop-portal、xvfb、xauth、wtype、
python3-dbus、python3-gi；Arch 對應 xdg-desktop-portal、
xorg-server-xvfb、xorg-xauth、wtype、python-dbus、python-gobject。
接著啟用並執行：

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON -DLUDASH_BUILD_PORTAL_RUNTIME_TESTS=ON
cmake --build build --target lunadash-portal lunadash-compositor
ctest --test-dir build --output-on-failure -R '^lunadash-portal-runtime$'
```

啟用後缺少依賴會直接造成 CMake 設定失敗，不會靜默跳過。包裝腳本建立
私人 runtime 目錄、X 顯示與 D-Bus；測試會另外啟動 headless Wayland
compositor 供 Slint 選擇器使用。前後端日誌與介面 XML 保留在
build/portal-runtime，選擇器截圖存放於 build/ci-evidence/。

## 驗證範圍

協議測試目前以 Qt Test 驗證 Qt D-Bus service；完整測試使用虛擬 X 顯示
啟動 portal frontend，並以 headless Wayland 顯示驗證兩個 Slint picker。這些
測試都不等於已驗證實機的
焦點、Flatpak 應用程式辨識或 NVIDIA／PipeWire
長時間串流。既有 Wayland 檢查驗證擷取協議與來源類型，不能當成已完成
Discord 直播驗收。分支 push 通過也不等於 PR 通過，還須確認同一版本的
PR 規則、儲存庫衛生、C++ 記憶體安全、Qt 生命週期與 CodeQL 檢查。

## 實機診斷

在出問題的 LunaDash 工作階段中，除了 compositor 日誌，也應擷取：

```sh
journalctl --user -b --no-pager -n 250 \
  -u xdg-desktop-portal.service \
  -u xdg-desktop-portal-lunadash.service \
  -u xdg-desktop-portal-wlr.service
```

用戶端的「Request ended (non-user cancelled)」不足以判斷哪個後端失敗。
「Unable to open /proc/<pid>/root」屬於應用程式辨識或權限問題，不能據此
認定顯示卡渲染失敗。不要用停用沙箱、放寬 ptrace 或給 Flatpak 全部主機
存取權來掩蓋錯誤；應從 journal 確認要求的 PID、實際後端與使用者 D-Bus。
