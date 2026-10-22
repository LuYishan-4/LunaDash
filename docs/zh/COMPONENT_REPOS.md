# 原始碼 repository 布局

LunaDash 根 repository 保留 compositor、桌面服務與建置整合。Quickshell UI 和 Astro 網站各自放在組織 repository，並以 Git submodule 固定在根 checkout。

| 路徑 | 來源 | 職責 |
| --- | --- | --- |
| `src/compositor/` | LunaDash 根 repository | 使用 LunaDash 共用 API 建置的 Wayland compositor |
| `qml/` | [LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) | 桌面工作階段載入的 Quickshell UI |
| `site/` | [LunaDash-docs](https://github.com/LunaDash-DE/LunaDash-docs) | 由根 Pages workflow 部署的 Astro 網站 |

根 repository 的每個 commit 都會在 `qml/` 和 `site/` 記錄確切的 QML 與網站 commit。組織 repository 有新 commit 時，LunaDash 使用的版本不會自動改變；必須更新根 repository 的 submodule 指標。C++ 宿主 API、翻譯、素材與 CMake 整合仍留在根 repository。

## Clone 與建置

Clone 根 repository 時一併取得固定版本的元件：

```sh
git clone --recurse-submodules --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
git submodule status
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

既有 checkout 可初始化或還原固定版本：

```sh
git submodule update --init --recursive
```

## 修改 Shell 或網站

先在 LunaDash-Shell 提交並 push QML 變更，或先在 LunaDash-docs 提交並 push 網站變更。接著更新根 repository 固定的版本，並將指標 commit 到目標為 `dev` 的分支：

```sh
git -C qml switch main
git -C qml pull --ff-only origin main
git -C site switch main
git -C site pull --ff-only origin main
git submodule update --remote qml site
git diff --submodule=log -- qml site
git add qml site
git commit -m "Update pinned shell and website"
git push
```

若只修改一個元件，只 stage 對應路徑。根 repo 的 commit 只包含新的 submodule 指標，不會複製元件檔案。相關的根 API、CMake、翻譯或技術文件仍在根 repository 修改。Compositor 使用 LunaDash 共用 API，因此目前不是獨立的 CMake 專案。
