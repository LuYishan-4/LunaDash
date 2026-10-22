# Repository 工作流程

LunaDash 的桌面、Quickshell UI、compositor 與 Astro 網站都放在同一個 Git repository。`src/compositor/`、`qml/`、`site/` 是 `dev` 分支內一般追蹤的目錄，不是 submodule。

| 路徑 | 職責 |
| --- | --- |
| `src/compositor/` | Wayland compositor，由根目錄 CMake 專案搭配 LunaDash 共用 API 建置 |
| `qml/` | Quickshell 桌面 UI，使用宿主提供的 C++ service、翻譯與資料 |
| `site/` | Astro 網站，由根 repository 的 Pages workflow 部署 |

Compositor 與 QML shell 會使用 LunaDash 其他目錄的 API 和素材。放在同一個 repository，就能在同一分支與 commit 一起更新相關程式。過去的 LunaDash-Shell、LunaDash-Compositor 與 LunaDash-docs repository 是歷史元件快照；一般開發請在本 repository 進行。

## Clone 與建置

Clone 開發分支並從根目錄建置：

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

## 修改程式

在同一個 LunaDash checkout 修改任意一個或多個目錄，再於根 repository 暫存、commit 並 push 分支，然後建立目標為 `dev` 的 pull request：

```sh
git switch -c feature/my-change
# 修改 src/compositor/、qml/、site/ 或其他專案路徑
git add src/compositor qml site
git commit -m "Describe the change"
git push -u origin feature/my-change
```

不需要另外建立元件 commit，也不需要更新 submodule 指標。網站保留自己的 `package.json`，可在 `site/` 目錄執行檢查；桌面與 Wayland 整合則從根目錄專案建置。
