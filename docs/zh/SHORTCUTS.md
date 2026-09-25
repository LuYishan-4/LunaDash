# 快捷鍵清單

[English](../en/SHORTCUTS.md)

以下為桌面預設值。**Meta／Super 通常是 Windows 鍵**。設定 → 鍵盤快捷鍵會顯示目前實際綁定，包含自訂及停用的項目；此表不會覆蓋已儲存的設定。共有 51 個可配置動作（31 個指令及 20 個工作區綁定），來源為 `src/desktop/shortcuts/ShortcutSettings.cpp`。

| 按鍵 | 動作 |
| --- | --- |
| Meta+H／L／K／J | 焦點向左／右／上／下 |
| Meta+Shift+H／L | 與左／右欄群組 |
| Meta+Ctrl+H／L | 將欄向左／右移動 |
| Meta+Shift+E | 將目前視窗移出群組 |
| Meta+Shift+C | 將欄置中 |
| Meta+=／Meta+- | 加寬／縮窄欄 |
| Meta+F | 最大化／還原 |
| Meta+Shift+F | 進入／離開全螢幕 |
| Meta+Shift+T | 切換浮動／平鋪 |
| Meta+C 或 Meta+Q | 關閉目前視窗 |
| Meta+M | 最小化目前視窗 |
| Meta+T 或 Meta+Enter | 開啟設定的終端機 |
| Meta+E | 開啟設定的檔案管理器，預設 Dolphin |
| Meta+D | 開啟應用程式搜尋 |
| Meta+A | 開啟 Orbit 啟動器 |
| Meta+Shift+S | 螢幕截圖 |
| Meta+W | 開啟桌布庫 |
| Meta+Ctrl+W | 隨機桌布 |
| Meta+N | 切換護眼模式 |
| Meta+`（反引號） | 顯示／隱藏暫存終端機 |
| Meta+I | 開啟控制中心 |
| Meta+V | 開啟剪貼簿歷史 |
| Meta+X | 開啟工作階段／電源選單 |
| Meta+1…9、Meta+0 | 切換工作區 1…9、10 |
| Meta+Shift+1…9、Meta+Shift+0 | 移動目前視窗至工作區 1…9、10 |

切換的目的工作區須已在設定中啟用。布局操作遵循目前模板；替換模板可能不支援群組或欄操作。

以下為固定操作，也列在設定 → 鍵盤快捷鍵頁面底部：

| 按鍵／手勢 | 動作 |
| --- | --- |
| 單按 Meta | 開啟應用程式搜尋 |
| F12 | 切換全螢幕 |
| Alt+Tab／Alt+Shift+Tab | 在視窗切換器向前／向後選擇 |
| 切換器內方向鍵 | 左右移動一項，上下移動五項 |
| 切換器內 Enter 或放開 Alt | 確認選擇 |
| 切換器內 Escape | 取消 |
| Alt+滑鼠左鍵拖曳 | 移動浮動視窗或調整平鋪排列 |
| Alt+Shift+滑鼠左鍵拖曳 | 調整視窗／布局大小 |

桌面介面內：應用程式搜尋使用上下鍵選擇、Enter 開啟、Escape 關閉。Orbit 搜尋欄使用 Tab／Shift+Tab 切換搜尋引擎、Alt+1…8 啟動項目；已聚焦的項目可用 Enter／Space 啟動，Escape 返回。設定使用 Ctrl+F 聚焦頂部搜尋，Escape 清空搜尋，搜尋已空白時關閉設定。快捷鍵錄製器使用 Enter／Space 開始錄製，錄製中 Escape 取消、Backspace 停用。已聚焦的工作區按鈕可用 Enter／Space 啟動。Escape 可關閉桌面右鍵選單及圖片選擇器。一般 Tab／Shift+Tab 導覽與外部應用程式自己的快捷鍵，由各控制項／應用程式負責。

內建歡迎工具保留 `welcome` 指令識別碼；視窗標題及啟動項目按鈕使用翻譯名稱，選擇繁體中文時顯示「歡迎」。
