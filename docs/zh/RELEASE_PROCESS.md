# Pull Request 與發布流程

[English](../en/RELEASE_PROCESS.md) · [繁中索引](README.md)

## Pull Request

一般貢獻 PR 以 `dev` 為目標。同一 repository 的 `dev` → `main` 升版 PR 明確允許；fork 與 feature branch 不適用。Policy 從 base commit 讀取，檢查 diff，不 checkout 或執行提案程式碼。

可見行為應同步更新 `docs/en/`、相同主題的 `docs/zh/` 與 `site/`。根 `docs/*.md` 保留為精簡的英文轉址頁。自動 gate 不要求無關的文件或網站修改。

一般 PR 不得修改 `.github/workflows/`、`site/src/pages/releases/` 下自動產生的 Markdown／MDX 或 `site/src/data/releases.json`。維護者在 `dev` 完成的 workflow 修改可透過同 repository 的升版 PR 帶到 main。Release impact 寫在 PR body。

[Dev CI](CI.md) 依完整 diff 選擇共用測試類別；[Main CI](CI.md)、排程及完整流程涵蓋全部測試與發行版，包含 NixOS、clang-tidy 及 CodeQL。小功能併入維護中的測試集，不各自新增 workflow。Branch protection 應要求 aggregate `CI result`，並查看升版 commit 的實際結果。略過的 suite 不代表已通過。

NixOS 發行內容包含根目錄 flake、lock 及 `nix/` 定義。系統世代更新方式及自動 runtime 驗證限制見 [NixOS](NIXOS.md)。

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
