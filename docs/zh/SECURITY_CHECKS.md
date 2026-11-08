# 安全性與崩潰檢查

[English](../en/SECURITY_CHECKS.md) · [繁中索引](README.md)

[Dev CI 與 Main CI](CI.md) 只負責建置。下列 source／repository 檢查請在本機執行。沒有任何 workflow 執行測試或分析工具。PR code 使用 GitHub-hosted runner，不使用 `pull_request_target`，checkout 不保留憑證。只有 Pages 部署使用 Pages／id-token 寫入權限。

| 檢查 | 主要覆蓋 | 失敗條件 |
| --- | --- | --- |
| Ubuntu build（CI） | 合成器、應用程式、SlintLu 與 plugin SDK 的 C/C++ 建置 | build 非 0 |
| Source architecture（`scripts/check-source-layout.py`） | 命名、dependency direction、GL ownership、CMake inventory | 任一違規 |
| Workflow policy/style（`check_repository_hygiene.py workflows`） | workflow permission、credential persistence、unsafe trigger | policy error |
| Secret/path scan（`check_repository_hygiene.py secrets`、`paths`） | token、private key、個人絕對路徑 | 高信心命中 |
| clang-tidy（`CMAKE_CXX_CLANG_TIDY`） | null/dangling、memory/lifetime、安全 API | compiler／analyzer 執行失敗；clang-tidy 診斷保留為 warning |

Static analysis 與 sanitizer 都不是安全證明；前者可能誤報/漏報，後者只涵蓋實際執行路徑。

## GitHub Rulesets

Workflow YAML 本身不能啟用 branch protection。Maintainer 應在 GitHub Rulesets / Branch protection 要求各分支的 aggregate `CI result`，拒絕失敗及意外略過的 suite。

## 本機檢查

```sh
python3 scripts/security/check_repository_hygiene.py workflows
python3 scripts/security/check_repository_hygiene.py secrets
python3 scripts/security/check_repository_hygiene.py paths

cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build-checked --parallel 4
```

要檢查記憶體與 undefined behavior 錯誤時，以 ASan 與 UBSan 建置專案程式碼：

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel 4
```

請在真實或巢狀 session 中執行 sanitizer 建置，實際走過要檢查的路徑。單純建置不等於驗證實體 GPU 驅動或完整的獨立登入 session。


## 1.0.1a UI／process 安全補充

Settings hardware probe 與 portal path 不會把使用者輸入拼成 shell command。FileChooser Location 只接受經 Qt 驗證的本機絕對路徑或本機 `file://` URL；system-tool 只從固定 executable/argument allowlist 啟動。

登出必須明確送出 `quit confirm`。Compositor 會把 clean event-loop shutdown 與 fatal signal 分開記錄，方便判斷 session 消失是正常登出還是真正 crash。Native plugin 仍在 compositor process 內執行，必須視為可信任程式碼。
