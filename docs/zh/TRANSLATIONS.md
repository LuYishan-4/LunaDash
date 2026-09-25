# 介面翻譯

[English](../en/TRANSLATIONS.md) · [繁中索引](README.md)

C++、QML、identifier、command argument、path、protocol value 保持英文。繁中介面文字位於：

```text
data/translations/zh_TW.json
data/translations/zh_TW/*.json
```

新 catalog 要列進 `cmake/LunaDashTranslations.cmake`。同一份合併後 dictionary 供 native Qt 與 Shell status 使用；英文或未知 app text fallback 到 source string。

## 程式碼用法

QML：

```qml
shell.tr("Settings")
```

C++：

```cpp
LunaDash::translate("Settings")
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

Translation review 也會 parse Shell QML、build localization implementation、比 embedded/source catalog、測 English fallback 與 dropdown label/value separation。

Translation 編進 Qt resource；修改後要重新 build 並 restart compositor，單獨 reload QML 不足以更新 compiled dictionary。

> 這個 `docs/zh/` 目錄是技術文件的繁體中文版本；它和 `data/translations/zh_TW/` 的**介面字串翻譯**是兩件不同的事。
