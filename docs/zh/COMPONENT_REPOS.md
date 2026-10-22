# 元件 repository

LunaDash 將網站、Quickshell Shell 與 compositor 原始碼放在獨立 GitHub repository。整合 repository 的 `dev` 以 Git submodule 固定各元件的確切 commit：

| 整合路徑 | Repository | 目前建置角色 |
| --- | --- | --- |
| `site/` | [LunaDash-docs](https://github.com/LunaDash-DE/LunaDash-docs) | 可獨立建置的 Astro 網站；由整合 repository 的 workflow 部署 |
| `qml/` | [LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) | 由整合後的 Quickshell runtime 載入的 QML 素材 |
| `src/compositor/` | [LunaDash-Compositor](https://github.com/LunaDash-DE/LunaDash-Compositor) | 由整合 repository 的 CMake 專案編譯 |

LunaDash 根 repository 仍負責整合。它的 CMake 檔案定義 targets，並將 compositor 與共用的 `src/config`、`src/core`、`src/desktop`、`src/shell` API 一起建置。因此 compositor repository 目前還不是獨立 CMake 專案。Shell QML 也依賴宿主提供的 C++ service、翻譯與資料素材。固定元件 commit 是版本邊界，不會自動更新，也不代表元件已能獨立執行。

## Clone 與建置

Clone 時一併取得 `dev` 選定的確切元件版本：

```sh
git clone --recurse-submodules --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
git submodule status
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

既有 checkout 可初始化或還原已記錄的元件版本：

```sh
git submodule update --init --recursive
```

一般建置不要使用 `git submodule update --remote`；它會將本機元件 checkout 移到更新的 commit，但不會修改整合 repository 記錄的版本。

## 修改元件並整合

先在元件 repository 修改、commit 並 push，再於整合 checkout 審查元件 commit。例如 QML 變更已提交至 `LunaDash-Shell` 後：

```sh
git -C qml fetch origin main
git -C qml checkout --detach <reviewed-component-sha>
git diff --submodule=log -- qml
git add qml
git diff --cached --submodule=log
git commit -m "Update pinned QML shell"
```

整合 repository commit 前，元件 commit 必須先 push，確保其他人能 clone 到它。整合指標 commit 到 `dev`；一般元件工作不需移動 `main`。網站、QML 與 compositor 原始碼各自需要對應的元件 commit，再更新整合 repository 的 pin。CI、網站部署、Nix 與 Arch source packager 都會初始化固定的 submodule。

若要快速更新至各元件設定的 `main` 分支，請先檢查取得的 commit ID：

```sh
git submodule update --remote site qml src/compositor
git diff --submodule=log
git add site qml src/compositor
git commit -m "Update pinned component revisions"
```

此指令只變更本機 checkout 與整合 repository 暫存的指標，不會在任何元件 repository 建立或推送 commit。

## 目前拆分界線

網站在自己的 Node package 中獨立檢查與建置。QML 可以獨立版本管理，但應用程式建置與行為測試仍在 LunaDash 執行，因為宿主 API 與測試素材位於整合 repository。Compositor 原始碼更依賴整合 repo 的共用 domain，因此 production CMake targets 與 Wayland runtime 測試仍由 LunaDash 負責。要宣稱 compositor 可獨立建置，還需要先把這些 API 移至穩定的公開 core。
