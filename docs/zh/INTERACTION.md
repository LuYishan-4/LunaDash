# 桌面互動與插件模板

## 快捷鍵與焦點

`Super+Tab`／`Super+Shift+Tab` 開啟十個工作區的總覽，放開 Super 確認。
`Alt+Tab`／`Alt+Shift+Tab` 改成只列出目前工作區已映射的應用程式視窗，
包含最小化的視窗；放開 Alt 聚焦並還原選取項目。這條路徑不會呼叫工作列
按鈕的最大化切換；已最大化工作區仍沿用既有的焦點／最大化規則。

兩種選擇器都支援方向鍵、滾輪、滑鼠選擇、Enter 確認與 Escape 取消。
若從其他方式切換工作區，會取消尚未確認的視窗選擇；關閉候選視窗也會
安全移除項目。這兩組快捷鍵與 Shift 反向組合均保留給選擇器；舊設定中
衝突的自訂綁定會遷移為 Disabled，不會默默攔走切換操作。

預覽沿用 compositor 最大 320×200 的縮圖擷取，不是持續影片串流。
無法取得縮圖時仍顯示圖示與標題，不影響選擇。

## 統一展開與收合

工作列與 Dock 的面板按鈕再次按下會收合。控制中心重按同一分頁按鈕會
收合，按另一分頁則切換內容。同時間只保留一個暫時展開的面板；左鍵點擊
桌布空白處或一般應用程式時收合，但不吞掉原本點擊。面板內部控制項及
其下拉選單不算外側點擊。事件經既有的私人互動通道送給 Shell，沒有新增
常駐、攔截所有輸入的全螢幕遮罩。

應用程式搜尋器不論從滑鼠或鍵盤開啟，都立即把輸入焦點交給搜尋欄。
在結果清單中繼續打字也會回到搜尋欄；關閉時釋放鍵盤焦點。內建介面的
被動滑鼠停留提示框已移除，無障礙按鈕名稱仍保留。

## 可替換的視窗與工作區模板

`window-switcher` 對應 Alt+Tab；`workspace-switcher` 對應 Super+Tab。
兩者提供 `context.interaction`、accent、background、foreground、muted
及 fontFamily。`interaction.scope` 為 windows 或 workspaces，資料分別
位於 `interaction.windows`／`interaction.workspaces`，index 從 0 起算。
視窗 ID 是 compositor ID，工作區總覽的 ID 為 1–10。

透過 `shell.command("switch-window", id)` 選取，再呼叫 switch-accept
或 switch-cancel。主機負責放開修飾鍵、生命週期與 Overlay；插件根物件
使用 Item，不另建 PanelWindow。可安裝的 SDK 2 範例位於
`templates/plugins/window-switcher`，安裝 SDK 時也會提供。插件失敗時
保留內建介面。

## 插件啟用／停用的全螢幕轉場

每次有效啟用狀態改變都產生轉場；單純輪詢、儲存相同值及首次載入不會。
同一插件套件的多個 target 合併成一個事件，不同插件的變更則依序播放。
同一個進度時鐘同步覆蓋所有螢幕；轉場沒有鍵盤焦點，輸入區域為空，
因此不會攔截點擊或打字。停用動畫／減少動態效果時會立即停止並清空佇列。

`plugin-transition` target 收到 context.progress（0–1）、change
（id、name、enabled、targets、previousTargets）、duration 與桌面色彩。
主機持有時間控制，動畫插件本身被停用也不會卡住桌面。內建時間可調為
100–1600 毫秒，預設 420。`templates/plugins/plugin-transition` 提供
全螢幕幕簾範例。轉場反映已接受的狀態變更，不會延後原生插件卸載，也
不是讓原生程式碼安全的隔離機制。

## 移除過時的自訂 QML

移除模組化頁面的自訂程式碼控制項、舊散落 QML 載入器、模板安裝功能，
以及 module-code-trust／module-template 指令。既有 JSON 的 custom
欄位僅為遷移而接受，正規化時捨棄；不再執行舊 QML，也不刪使用者檔案。
模組的 enabled、style、config 繼續使用原本的 schema。新介面程式碼
統一透過有 metadata 的 SDK 2 插件。繁中插件用語統一為「插件」，一般
模組設定仍稱「模組」。

## 驗證

來源結構、QML 設計／動作及輸入規則都有檢查；CTest 加入視窗範圍、候選
移除、取消及快捷鍵衝突測試。QML 測試涵蓋收合政策、插件狀態差異，以及
範例模板的畫面與指令。原生 Wayland 鍵盤、快速連點及實際 Shell 截圖仍
須由相關執行測試或實機確認。
