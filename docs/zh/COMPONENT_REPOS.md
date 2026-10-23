# 元件 repository

LunaDash 根 repository 保留 native desktop 與 Wayland compositor。QML shell 和 Astro 網站各自有獨立的原始碼 repository；LunaDash checkout 不含這兩個 repository 的原始碼。

| 元件 | Repository | LunaDash 的使用方式 |
| --- | --- | --- |
| Native desktop 與 compositor | [LunaDash](https://github.com/LuYishan-4/LunaDash) | C++20、C11 rendering core、Wayland 整合與 packaging |
| Quickshell UI | [LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) | 獨立安裝 QML 原始碼；LunaDash 使用的 commit 記錄於 `packaging/shell-revision` |
| Astro 網站 | [LunaDash-docs](https://github.com/LunaDash-DE/LunaDash-docs) | 在該 repository 執行 website CI；LunaDash workflow 只在 runner 暫存目錄 clone |

只有根 `dev` branch 的 `packaging/shell-revision` 改變時，LunaDash 才會採用新的 Shell commit。Shell repository 有新 commit 不會自動更新既有 LunaDash checkout 或安裝版本。網站有自己的歷史，不由 LunaDash 固定 revision；網站檢查和 Pages 部署會抓取 LunaDash-docs 的 `main` branch。

## Clone 與安裝

根 checkout 不使用遞迴 submodule：

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

一般安裝指令會抓取固定版本的 Shell source，並與 LunaDash 一起安裝。若直接用 CMake 建置，先抓取選定的 Shell revision，再傳入 checkout 路徑：

```sh
bash scripts/fetch-shell.sh ../LunaDash-Shell
cmake -S . -B build -G Ninja -DLUDASH_QML_SOURCE_DIR="$PWD/../LunaDash-Shell"
cmake --build build --parallel 2
```

LunaDash-Shell 也能不依賴整合 repository 單獨安裝：

```sh
git clone https://github.com/LunaDash-DE/LunaDash-Shell.git
cd LunaDash-Shell
./install.sh
```

獨立安裝腳本預設把 Shell UI 檔案安裝到使用者資料目錄。使用 `sudo ./install.sh --prefix /usr` 可安裝到系統路徑；這不會安裝 Quickshell 或 LunaDash native services。

## 修改元件

在 LunaDash-Shell checkout 修改 QML。將變更 commit 並 push 到其 `main` branch；若要讓 LunaDash 採用新版本，再更新 LunaDash `dev` 的 Shell revision：

```sh
git -C ../LunaDash-Shell switch main
git -C ../LunaDash-Shell pull --ff-only origin main
git -C ../LunaDash-Shell rev-parse HEAD
```

把確認過的 40 字元 Shell commit 寫入 LunaDash 的 `packaging/shell-revision`。Nix flake 會讀取同一個檔案。在 LunaDash `dev` commit 這項變更後，CI 會抓取選定的 Shell commit 並執行 native 與 QML suites。

網站修改放在 LunaDash-docs。在該 repository 執行 `npm ci`、`npm run check`、`npm run build` 和 `python3 scripts/test-site.py dist`，再 commit 並 push 到 `main`。根 Pages workflow 部署時會抓取該 branch，不必在 LunaDash 更新網站指標。

Compositor API、翻譯、native 素材、CMake 整合與技術文件仍放在 LunaDash。Compositor 使用 LunaDash 共用 API，因此不是獨立的 CMake 專案。
