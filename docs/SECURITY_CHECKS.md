# PR 資安與 crash 檢查

每次 `pull_request`、`push`、`merge_group` 或手動啟動都執行完整工作流程，沒有以路徑過濾略過核心檢查。PR 程式碼只在 GitHub-hosted runner 執行，不使用 `pull_request_target`，checkout 不保留憑證。僅 CodeQL job 有上傳安全報告的 `security-events: write` 權限。

| 檢查 | 目的 | 失敗條件 |
| --- | --- | --- |
| Linux build | Arch / Ubuntu / Fedora 編譯與互動測試 | 編譯或測試非零退出 |
| C++ static analysis | 空指標、懸空引用、use-after-move、可疑記憶體操作與安全 API | clang-tidy 啟用的任何警告 |
| ASan and UBSan | 測試路徑內越界、use-after-free、double-free、未定義行為 | sanitizer 報告或測試失敗 |
| CodeQL security and quality | C/C++ 資料流、安全與品質查詢 | SARIF 存在 error/warning 或 security-severity 結果；沒有報告亦失敗 |

CodeQL 使用實際 CMake build 建立資料庫，以涵蓋 Qt 與 moc。一般 CodeQL 分析成功不等於沒有漏洞，因此另外執行 `scripts/security/check_sarif.py`，讓分析發現也確實影響 job 結果。檢查腳本不輸出原始程式碼片段。

Ubuntu sanitizer job 使用 `--no-shell` 測試原生用戶端生命週期；Arch job 額外驗證 Quickshell layer-shell 介面。

Qt／Mesa 的程序級別配置可能造成外部 library 的 leak 回報，ASan 測試暫設 `detect_leaks=0`；這**不檢查 memory leak**，其他記憶體安全檢查仍開啟。ASan 與 UBSan 只能發現已執行路徑的問題，CodeQL／clang-tidy 也不是完整安全性證明。

CodeQL 的 GitHub code scanning 需公開 repository 或符合 GitHub Code Security 授權與權限的私人 repository。workflow 會直接顯示設定／授權失敗，不會假裝已通過。

## GitHub 必須通過的檢查

推送到 GitHub 後，在 repository 的 Rulesets / Branch protection 設定主分支禁止略過以下檢查：

- Linux build 的三個 matrix checks。
- `C++ static analysis`。
- `ASan and UBSan`。
- `CodeQL security and quality`。

YAML 本身不能替 repository 管理者開啟分支保護。此專案目前只提供工作流程與設定指引，沒有宣稱遠端已啟用。

## 本機

先完成程式碼與文件，再執行：

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-asan -j
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 QT_QPA_PLATFORM=xcb xvfb-run -a ctest --test-dir build-asan --output-on-failure

cmake -S . -B build-tidy -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_CLANG_TIDY=clang-tidy -DBUILD_TESTING=OFF
cmake --build build-tidy -j
```

參考：[CodeQL 編譯語言設定](https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages)、[AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)、[UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)、[clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html)。
