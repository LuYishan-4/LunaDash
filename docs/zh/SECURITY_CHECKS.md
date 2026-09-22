# 安全性與崩潰檢查

[English](../en/SECURITY_CHECKS.md) · [繁中索引](README.md)

核心 build/runtime workflow 會在對應 push、PR、merge-group 或手動 dispatch 執行；較昂貴的 PR analyzer 依路徑啟動。Pull-request code 使用 GitHub-hosted runner，沒有 `pull_request_target` 執行不受信任 PR code 的設計。Website deploy 是少數需要 Pages/id-token 權限的 workflow。

| 檢查 | 主要覆蓋 | 失敗條件 |
| --- | --- | --- |
| Ubuntu build | C/C++、Files test、architecture | build/test 非 0 |
| Source architecture | 命名、dependency direction、GL ownership、CMake inventory | 任一違規 |
| Workflow policy/style | workflow permission、credential persistence、unsafe trigger | policy error |
| Secret/path scan | token、private key、個人絕對路徑 | 高信心命中 |
| Source language/shell | 英文 source policy、shell syntax/shellcheck | violation |
| clang-tidy | null/dangling、memory/lifetime、安全 API | project/test source 的 enabled warning |
| CodeQL | C/C++ security/quality/dataflow | SARIF error 或帶 security-severity 的 finding；一般 quality warning 保留 annotation；缺少 report 也失敗 |
| Graphics diagnostics | shader variant、graphics pipeline log | pipeline/shader failure |

CodeQL 會用真的 CMake/Qt/moc 建 database。Analysis job 顯示完成不代表「沒有 finding」；`scripts/security/check_sarif.py` 會把帶有 security-severity 的 finding 或明確 SARIF error 轉成 job failure。非安全性的 quality warning 仍會顯示在 CodeQL annotation，但不再單獨讓 PR 失敗。

Ubuntu 24.04 的 Qt lifetime 測試仍保留 `clang-analyzer-cplusplus.NewDelete`。測試只接受 synthetic project source 本身的診斷，因此 Clang 18 若把 valid `QPointer` 的主要診斷位置落在 Qt system header，不會再誤判成 LunaDash 的 use-after-free；刻意建立的 invalid lifetime 仍必須被 analyzer 擋下。

Static analysis 與 dynamic test 都不是安全證明；前者可能誤報/漏報，後者只涵蓋實際執行路徑。

## GitHub Rulesets

Workflow YAML 本身不能啟用 branch protection。Maintainer 應在 GitHub Rulesets / Branch protection 明確要求需要的 build/runtime/website/architecture/analysis checks。

## 本機檢查

```sh
python3 tests/security/test_analyzer.py clang-tidy

cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build-checked --parallel 4

LUDASH_GRAPHICS=opengl LUDASH_TEST_NO_SHELL=1 \
  LUDASH_BUILD_DIR="$PWD/build-checked" ./scripts/test-wayland.sh
```

最後一個是 headless wlroots/pixman session，不等於實體 GPU 測試。Separate software OpenGL regression 請看 [Graphics](GRAPHICS.md)。


## 1.0.1a UI／process 安全補充

Settings hardware probe 與 portal path 不會把使用者輸入拼成 shell command。FileChooser Location 只接受經 Qt 驗證的本機絕對路徑或本機 `file://` URL；system-tool 只從固定 executable/argument allowlist 啟動。

登出必須明確送出 `quit confirm`。Compositor 會把 clean event-loop shutdown 與 fatal signal 分開記錄，方便判斷 session 消失是正常登出還是真正 crash。Native plugin 仍在 compositor process 內執行，必須視為可信任程式碼。
