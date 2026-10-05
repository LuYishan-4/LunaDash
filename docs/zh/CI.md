# 持續整合

[English](../en/CI.md) · [繁中索引](README.md)

`dev-ci.yml` 與 `main-ci.yml` 是不同入口，共用 `main-build.yml`。功能測試放在既有測試集，不再為個別功能新增 workflow。

| 分支／事件 | 檢查範圍 |
| --- | --- |
| `dev` push 或 PR | 每次執行 repository／source contracts；依完整 Git diff 選擇 Ubuntu runtime、QML、website、Nix 與 Arch |
| `main` push 或 PR | 所有共用測試、五個發行版建置、clang-tidy／Qt lifetime、CodeQL |
| Merge queue | 全部測試類別及五個發行版建置 |
| 每週排程 | 在預設分支 `main` 執行完整檢查 |
| 手動 `Dev CI` | 依最後一個 commit 選擇測試；勾選 `full` 執行全部檢查 |

CI／policy 修改、未知原始碼目錄、新分支或無法取得基準 commit 時會擴大檢查，包含完整發行版 matrix，不會默默跳過。改名與刪除都納入選擇。`dev` 的純文件修改不會建置桌面；網站修改會執行 Astro 與連結檢查。`main` 即使只有文件變動仍執行完整檢查。`tests/ci/test_changes.py` 驗證這些邊界。

## 共用測試集

- **Repository／source contracts：** 原始碼布局、英文 source、shell syntax／ShellCheck、QML action／design、翻譯、installer／source archive 與 hygiene。獨立步驟在其他步驟失敗後仍會回報。
- **Ubuntu build／runtime：** 一次有快取的 CMake 建置，供 CTest（包含真實 portal frontend）、plugin SDK 安裝、啟動／linkage、Wayland／XWayland 與安裝後的 software OpenGL 使用。各 runtime 步驟共用建置結果並獨立回報。
- **QML：** 解析所有 shell QML，執行整個 Qt Quick 測試目錄。
- **Website：** Astro 型別檢查、正式建置、素材與連結檢查。
- **NixOS：** 鎖定套件、模組求值與安裝後的 headless runtime；x86_64 實際建置，aarch64 僅求值。見 [NixOS](NIXOS.md)。
- **發行版：** 開發變更執行 Arch；完整流程加入 Debian 13、Fedora 44、openSUSE Tumbleweed 與 Alpine Edge。
- **安全分析：** Qt lifetime fixtures、clang-tidy production build、CodeQL 與 SARIF gate。在完整流程或 CI／security 設定修改時執行，平常開發原始碼 push 不再重複兩次 analyzer 建置。

同一 PR／分支的新執行會取消舊執行。每個 job 有時限，runtime 證據保留為 artifact。`CI result` 拒絕失敗、取消或應執行卻被跳過的 suite；只有刻意未選取的 suite 可以略過。可在 GitHub ruleset 要求各分支的 aggregate result；YAML 不會自行修改 branch protection。

Push 從同分支、同工作流程最近一次成功的提交比較，避免取消或失敗的前一輪隱藏尚未驗證的修改。若 GitHub 無法提供這個基準，就執行全部測試類別。

一般 PR 仍以 `dev` 為目標，允許正常審查 workflow 修改，並保護自動產生的 release notes。同一 repository 的 `dev` → `main` 升版 PR 明確允許已審查的 workflow 修改；fork 內名為 `dev` 的分支不適用。Policy 使用 PR base commit 的程式碼。

## 腳本與驗證紀錄

移除未使用的 `scripts/security/check_pr_scope.py`（舊版禁止修改文件規則）、`scripts/security/run_local_audit.sh`（重複 audit／build wrapper）及 `scripts/test-xdpw-sources.sh`（無引用的獨立 probe）。仍在使用的 installer、session、SDK、diagnostics 與文件記載的手動工具保留。

成功結果只證明該 commit 實際執行的檢查。實體 GPU／輸入、真實登入、Quickshell 外觀及應用程式相容性仍需發行前測試。詳見[測試指南](TESTING_AND_FILES.md)與[安全檢查](SECURITY_CHECKS.md)。`dev` 會驗證網站原始碼；公開 Pages 網站由 `main` 部署。

翻譯 gate 涵蓋所有隨附語言。啟用共用檢查時，一併補齊既有桌面字串缺漏並移除重複的語言包鍵。

Runtime 測試會等待 document portal 卸載 FUSE，再清除暫存目錄。通知生命週期測試為首次延遲啟動的 XWayland／GLX 保留較長時限，仍保留全部 25 次關閉與映射檢查。

允許對 dev 提交 workflow 修改並接受正常審查，仍保護自動產生的發行資訊。Ubuntu CI 加入 GTK 3／4 與 Qt 生命週期測試並保留截圖。Fork 的 CodeQL 仍分析及檢查 SARIF，但不使用唯讀 token 上傳 security events。Fedora 使用穩定的 44，而非 45 beta，參見 [Fedora 發行頁](https://fedoraproject.org/)。

Arch 發行版工作使用同一次建置的執行檔測試首次登入 Quickshell：擷取 Welcome、透過 Wayland 虛擬鍵盤啟用「開始桌面」，再重新啟動驗證設定持久性。發行版 artifact 保留此軟體工作階段截圖與紀錄。
