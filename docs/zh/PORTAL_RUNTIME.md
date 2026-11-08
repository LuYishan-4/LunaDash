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

## 驗證範圍

沒有任何 workflow 執行 portal 後端；CI 只負責建置。實機的焦點、Flatpak 應用程式
辨識與 NVIDIA／PipeWire 長時間串流都必須在實機工作階段中確認。支援擷取協議與
來源類型，不能當成已完成 Discord 直播驗收。

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
