# Files 與檔案關聯

[English](../en/FILES.md) · [繁中索引](README.md)

LunaDash Files 是原生 Qt 介面。UI、interaction、association 與 file operation 分別放在 `src/desktop/filemanager/`、`src/desktop/fileoperations/`。

## 第一次開檔與 Open with

第一次遇到尚未看過的 file type 時，Files 預設會詢問是否選 application。雙擊文件或選同類多個檔案後使用 **Open with…**，chooser 會優先顯示 recommended/system-default handler，並可搜尋或顯示全部 installed applications。

**Always use this application** 未勾選時只開一次；已儲存 handler 如果後來移除，會重新詢問而不是靜默失敗。Directory 永遠留在 Files。Desktop entry / binary 由明確選定的 installed handler 啟動，filename 以 argument 傳入，**不把路徑拼成 shell command**。

規則原子寫入：

```text
$XDG_CONFIG_HOME/LunaDash/file-associations.json
```

一般為 `~/.config/LunaDash/file-associations.json`。例：

```json
{
  "version": 1,
  "initialized": true,
  "askOnFirstOpen": true,
  "associations": {
    "ext:txt": {
      "desktopId": "org.example.Editor.desktop",
      "mimeType": "text/plain"
    }
  }
}
```

Extension 不分大小寫，compound extension 取最長 match；無 extension 用 `mime:<type>`。Files 自己的 extension rule 不會自動改 system MIME default。只有使用者明確勾 **Also make it the system default** 時才透過 GIO 寫 system association。

## 選取、選單與快捷鍵

Icon/details view 共用 selection。右鍵已選項會保留 multi-selection；右鍵未選項只 target 那列；右鍵空白 target current directory。

主要快捷鍵：

- `Ctrl+C/X/V/A`：copy/cut/paste/select all
- `Ctrl+Shift+C`：copy paths
- `F2`：rename
- `Delete`：Trash（有確認）
- `Enter`：open
- `Alt+Enter`：properties
- `Ctrl+N`：new Files window
- `Ctrl+Shift+N`：new folder
- `Ctrl+Alt+T`：terminal here
- `Alt+Left/Right/Up`、`Ctrl+L`、`F5`：navigation/address/refresh

Clipboard 使用 local file URI 與 KDE/GNOME cut marker，不是 Files 私有 clipboard。

## Copy / move / drag-drop

將 local file 拖到 directory 預設 copy，按 Shift 可要求 move。Remote URL 不會假裝成 local file。

Copy 支援 directory、hidden entry、symlink（不 follow link）。先寫 temporary destination，再用 Linux `renameat2(RENAME_NOREPLACE)`，避免 incomplete copy 覆蓋既有目標。會拒絕：

- 已存在 target
- duplicate target name
- copy 到自己
- 同時選 directory 與其 descendant
- FIFO/device node 等特殊檔

Operation 非同步；開始後 navigation 不會改該 operation 已捕捉的 destination。沒有跨多 entry 的全域 undo/transaction，因此若中途錯誤，前面的 entry 可能已完成。

跨 filesystem move 目前不會自動 copy-then-delete，而是失敗並保留 source。Permissions/mtime 會盡量保留，但 ownership、ACL、xattr 不承諾。Permanent delete、archive editor、recursive size、remote mount 尚未實作。

## 測試

```sh
cmake -S . -B build -DLUDASH_BUILD_FILES_TESTS=ON
cmake --build build --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R '^lunadash-files$'
```

測試使用隔離 XDG config/desktop entries，覆蓋 association、corrupt-config preservation、first-use prompt、clipboard、directory/symlink、no-overwrite、recursion、special-file rejection、same-filesystem move 與含 shell metacharacter 的 launch arguments。
