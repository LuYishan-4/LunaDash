# 舊檔案關聯設定遷移

[English](../en/FILE_ASSOCIATION_MIGRATION.md)

新版 Files 第一次啟動時，會把舊 QSettings 的 `fileAssociations/ext_*` 與 `fileAssociations/mime_*` 匯入 `$XDG_CONFIG_HOME/LunaDash/file-associations.json`。

- 已存在的 JSON rule 優先。
- 舊 key 保留作 backup。
- Migration-version marker 避免使用者刪掉 JSON rule 後又被重複匯入。
- 不存在或無法辨識的 desktop entry 不會直接執行；下次 open 時重新詢問。

`__system__` rule 表示「跟隨目前 system MIME handler」，不是把 executable 固定下來。Chooser 也提供 **Use the current system default**。如果 handler 後來移除，就再次顯示 chooser。

右鍵 **Set default application for this file type…** 可只儲存 rule，不立即開文件；**Reset default application for this file type** 只刪 Files rule，不會回復 system-wide MIME default。

早期版本可能在儲存 extension rule 時一併改 system MIME；遷移不會自動反轉那些系統設定。新版只有使用者明確選 system-default 選項時才修改。
