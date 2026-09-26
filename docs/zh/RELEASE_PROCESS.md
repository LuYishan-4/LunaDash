# Pull Request 與發布流程

[English](../en/RELEASE_PROCESS.md) · [繁中索引](README.md)

## Pull Request

一般 PR 必須以 `dev` 為 target branch。現有 PR policy gate 會拒絕一般 feature/fix PR 直接送往其他 base。

每個 PR 都要同步更新：

- `docs/en/`：英文技術／使用者文件。
- `docs/zh/`：相同主題的繁體中文版本。
- `site/`：對外網站中對應的 guide、API 或產品說明。

共享圖片與品牌資源仍在 `docs/image/`、`docs/brand/`。根 `docs/*.md` 目前是語言目錄遷移期的相容副本，不應成為新連結的首選。

一般 PR 不得新增、修改、刪除或 rename：

- `.github/workflows/`
- `site/src/pages/releases/` 下的 release Markdown/MDX
- `site/src/data/releases.json`

Release impact 寫在 PR body；公開 release notes 由 GitHub Releases 產生。

CI 目前保留廣義主流程：Main build and integration、跨發行版建置、網站，以及 PR policy/style/security。單一小功能不再各自維護 workflow；其 regression 應併入 maintained test suite 或 release 實機驗證。被 path filter 跳過的 job 不能宣稱「測試通過」。

## Release notes

**GitHub Release Markdown 是唯一 canonical release-note source。** Pages deployment 不需要把產生的 release Markdown commit 回 repo：

1. GitHub Release publish/edit/delete。
2. `site-pages.yml` checkout 最新 `main` 網站。
3. `scripts/site/sync-releases.py` 讀 GitHub Releases API。
4. 在 deployment workspace 產生 `site/src/pages/releases/<tag>.md`。
5. 重建 `site/src/data/releases.json`。
6. Astro check/build 後部署 GitHub Pages。

公開路徑：

```text
/releases/
/releases/<tag>/
```

建議 Release body：

```markdown
## Highlights
- 主要使用者可見變更。

## Desktop and shell
- UI / compositor / window / settings 細節。

## Fixes
- 重要修正。

## Upgrade notes
- migration、restart requirement、known limitation。
```

## Branch flow

```text
feature/fix -> PR -> dev -> stabilization -> main -> release tag
```

`main` 是 release branch；一般 feature/fix 不直接 target main。

## Release 前架構檢查

Promotion 前要查看**同一個 commit SHA**的 build/runtime/website/source architecture/distro workflow。另建 source archive 與 staged install，確認 executable、compat alias、session/portal entries、QML、translation、Plugin SDK template/example 與 embedded shaders 都在安裝結果中。

文件只能宣稱實際驗證過的範圍。Software OpenGL/pixman/distro source build 不能推論所有 physical GPU、input seat、Chrome/Zed、多輸出或 display-manager 一定正常。
