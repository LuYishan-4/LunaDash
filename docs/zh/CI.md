# 持續整合

[English](../en/CI.md) · [繁中索引](README.md)

CI 只負責建置。測試、contract 檢查與 runtime 工作階段請在本機用 `scripts/run-tests.sh` 執行
（見[測試](TESTING.md)），push 前先跑一次。

`dev-ci.yml` 與 `main-ci.yml` 是不同入口，共用 `main-build.yml`。

| 分支／事件 | 建置範圍 |
| --- | --- |
| `dev` push 或 PR | 依完整 Git diff 選擇 Ubuntu、website、Nix 與 Arch |
| `main` push 或 PR | 全部建置，含五個發行版 |
| Merge queue | 全部建置，含五個發行版 |
| 每週排程 | 在預設分支 `main` 執行全部建置 |
| 手動 `Dev CI` | 依最後一個 commit 選擇建置；勾選 `full` 建置全部 |

CI／policy 修改、未知原始碼目錄、新分支或無法取得基準 commit 時會選擇全部建置與完整發行版 matrix。
改名與刪除都納入選擇。`dev` 的純文件修改不會建置桌面；網站修改會建置網站。選擇邏輯在
`scripts/ci/changes.py`（`tests/ci/test_changes.py` 驗證）。

## 建置項目

- **Ubuntu：** 一次有快取（ccache、Slint 的 Cargo 輸出、SlintLu）的 CMake 建置，涵蓋合成器、應用程式、
  SlintLu 與 plugin SDK，使用 `-DBUILD_TESTING=OFF`。
- **Website：** Astro 網站的正式建置。
- **NixOS：** x86_64 套件的 `nix build`。Slint 與 SlintLu 是獨立 derivation，Nix store 會在執行之間快取，
  只重建有變更的 derivation；沒有快取時可能超過一小時。見 [NixOS](NIXOS.md)。
- **發行版：** 開發變更建置 Arch；完整流程加入 Debian 13、Fedora 44、openSUSE Tumbleweed 與 Alpine Edge。
  各自以 `scripts/install-dependencies.sh` 安裝相依套件並建置 release 設定。

沒有任何 workflow 執行測試、linter 或分析工具；`pr-gate.yml` 只檢查 PR 規範。

同一 PR／分支的新執行會取消舊執行，每個 job 都有時限。建置錯誤會轉成 annotation
（`scripts/ci/report_build_errors.py`）。`CI result` 拒絕失敗、取消或應執行卻被跳過的建置；只有刻意未選取的
建置可以略過。可在 GitHub ruleset 要求各分支的 aggregate result；YAML 不會自行修改 branch protection。

Push 從同分支、同工作流程最近一次成功的提交比較，避免取消或失敗的前一輪隱藏尚未建置的修改。若 GitHub
無法提供這個基準，就建置全部。

貢獻 policy（`pr-gate.yml`）仍以 `dev` 為目標，允許正常審查 workflow 修改，並保護自動產生的 release notes。
同一 repository 的 `dev` → `main` 升版 PR 明確允許已審查的 workflow 修改；fork 內名為 `dev` 的分支不適用。
Policy 使用 PR base commit 的程式碼。

CI 通過只代表該 commit 能夠建置。實體 GPU／輸入、真實登入、殼層外觀及應用程式相容性仍需發行前測試。
詳見[測試指南](TESTING_AND_FILES.md)與[安全檢查](SECURITY_CHECKS.md)。公開 Pages 網站由 `main` 部署。
