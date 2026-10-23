# 介面翻譯

[English](../en/TRANSLATIONS.md) · [繁中索引](README.md)

C++、QML、identifier、command argument、path、protocol value 保持英文。繁中介面文字位於：

```text
data/translations/zh_TW.json
data/translations/zh_TW/*.json
```

新增 catalog 直接放進對應 locale 目錄；native loader 會依語系自動讀取，安裝流程已包含整個目錄。同一份合併後 JSON dictionary 供 Shell status 與 Slint Welcome、套件管理器和 portal client 使用；英文或未知文字 fallback 到 source string。

目前選用語系儲存在原生桌面偏好。載入器會把舊 `LunaDash.conf` 的語系欄位當作遷移 fallback；重設桌面偏好時會忽略這個舊值，但保留舊檔。

## 程式碼用法

QML：

```qml
shell.tr("Settings")
```

有 placeholder 時要**先翻譯再代入**：

```qml
shell.tr("Switch to workspace %1").arg(number)
```

所有語言都必須保留相同 placeholder。不要翻譯 serialized shortcut、channel ID、font-family value、filename、application 提供的 notification 或 raw diagnostic。

`StyledComboBox.translationContext` 可以只翻 visible label/popup rows，而不改 model、`currentText`、`currentValue`。Model 應保存 source label，不要每次 status snapshot 都重建 translated array。Settings search 會同時索引 source 與 translated alias。

## 驗證

```sh
python3 scripts/check-translations.py
python3 scripts/test-translation-checker.py
```

檢查內容包含 catalog JSON、literal translation call、常見 UI text property、部分 model label、placeholder parity、conflicting duplicate 與 cross-catalog duplicate。這不是 semantic proof，runtime dynamic string 或第三方 app text 仍可能沒有翻譯。

Translation review 也會 parse Shell QML、在不連結 Qt 的情況下 build native localization、比較載入的 dictionary 與 source catalog、測 English fallback 和語系偏好遷移，並測 dropdown label/value separation。

Translation catalog 是一般 JSON 檔。開發時從 source tree 讀取，安裝後從 `${datadir}/ludash/translations` 讀取。更新已安裝 catalog 不需重新 build LunaDash；重新啟動受影響的 client 或 compositor 即可重新載入 dictionary。

> 這個 `docs/zh/` 目錄是技術文件的繁體中文版本；它和 `data/translations/zh_TW/` 的**介面字串翻譯**是兩件不同的事。
