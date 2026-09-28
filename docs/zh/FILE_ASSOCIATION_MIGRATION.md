# 改用 Dolphin 後的檔案關聯

LunaDash 內建檔案管理器及其專用關聯設定已退役。Dolphin 和其他自訂外部檔案管理器使用各自偏好與系統 MIME 關聯。

LunaDash 不再讀取或遷移 `$XDG_CONFIG_HOME/LunaDash/file-associations.json` 及較早的 QSettings `fileAssociations/*` 設定。既有檔案保留供參考，不會自動匯入 Dolphin，也不會刪除。需要時請透過 Dolphin 或系統預設程式設定重新選擇文件開啟程式；先前已修改的系統 MIME 關聯仍有效。

Settings > Applications and startup 仍可調整 files 角色，空白命令表示 Dolphin。FileChooser 與螢幕／視窗分享選擇器繼續由 LunaDash 提供，詳見[檔案管理](FILES.md)。
