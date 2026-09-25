# LunaDash 文件 — 繁體中文

[English](../en/README.md) · [文件總索引](../README.md)

- [NyxNiri 桌面整合、Orbit 與動態桌布](NYXNIRI_DESKTOP.md)
這裡放置與 `dev` 分支同步的繁體中文技術文件。LunaDash 目前仍是開發預覽版；CI、巢狀工作階段或軟體繪圖測試通過，不代表所有實體 GPU、登入管理器、輸入裝置、應用程式與多螢幕環境都已完成驗證。

## 建議先看

- [安裝、登入與救援](LOGIN_SESSION.md)
- [設定、快捷鍵與桌面行為](SETTINGS.md)
- [建置、測試與原始碼地圖](TESTING_AND_FILES.md)
- [原始碼架構](ARCHITECTURE.md)

## 桌面功能

- [完整快捷鍵清單](SHORTCUTS.md)

- [首次啟動與設定](CONFIGURATION.md)
- [顯示器控制與啟動流程](DISPLAY_AND_STARTUP.md)
- [視窗操作](WINDOWS.md)
- [視窗配置模板](WINDOW_LAYOUT_TEMPLATES.md)
- [語言與輸入法](INPUT_METHODS.md)
- [XWayland 相容層](XWAYLAND.md)
- [視窗動畫與效果](EFFECTS.md)
- [螢幕截圖](SCREEN_CAPTURE.md)
- [媒體面板](MEDIA.md)
- [Shell 繪製](SHELL_RENDERING.md)

## 擴充 LunaDash

- [Shell 模組](MODULES.md)
- [Plugin SDK 2](PLUGINS.md)
- [Plugin target 參考](PLUGIN_TARGETS.md)
- [C 核心與 C++ 整合](C_CORE.md)
- [圖形與 renderer 資源](GRAPHICS.md)

## Files、維護與專案流程

- [Files 與檔案關聯](FILES.md)
- [預設應用程式與 Files](DEFAULT_APPS_AND_FILES.md)
- [檔案關聯遷移](FILE_ASSOCIATION_MIGRATION.md)
- [介面翻譯](TRANSLATIONS.md)
- [安全與崩潰檢查](SECURITY_CHECKS.md)
- [發布流程](RELEASE_PROCESS.md)
- [網站與 GitHub Pages](WEBSITE.md)
- [測試快速入口](TESTING.md)

## 目前 dev 1.0.1a 已涵蓋的重點

目前文件會跟進 wlroots 合成器與生命週期拆分、卡片式 Dashboard、可最大化 Settings、LunaDash 風格 FileChooser 與螢幕分享來源選擇器、受邊界限制的平鋪與可選 stacking、Plugin SDK 2、Wallpaper Background layer 的桌面 widget（Digital Clock / Audio Wave）、MPRIS、亮度/DDC/CI、擷取與 XWayland。

- [共用設定 API 與自動控制項](SETTINGS_API.md)
