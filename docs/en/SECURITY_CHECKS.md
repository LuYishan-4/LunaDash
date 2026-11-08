# Security and crash checks

[Dev CI and Main CI](CI.md) only build. The source and repository checks below run locally. No workflow runs tests or analyzers. Pull-request code runs on GitHub-hosted runners without `pull_request_target`, and checkout does not retain credentials. Only Pages deployment receives `pages: write` and `id-token: write`, and never runs PR code.

| Check | Coverage | Failure condition |
| --- | --- | --- |
| Ubuntu build (CI) | Ubuntu 24.04 C/C++ build of the compositor, apps, SlintLu and plugin SDK | Nonzero build exit |
| Source architecture (`scripts/check-source-layout.py`) | Naming, dependency direction, GL ownership and explicit source/resource inventory | Any architecture violation |
| Workflow policy and style (`check_repository_hygiene.py workflows`) | Workflow structure, permissions, credential persistence and unsafe triggers | Policy or style violation |
| Credential and personal path scan (`check_repository_hygiene.py secrets`, `paths`) | API keys, tokens, private keys and user-specific filesystem paths in source/configuration | Any high-confidence match |
| C/C++ static analysis (`CMAKE_CXX_CLANG_TIDY`) | Null/dangling pointers, suspicious memory operations, use-after-move, security APIs | Compiler/analyzer execution failure; clang-tidy diagnostics remain warnings |

Sanitizers cover only executed paths; static analysis is not a proof that the application is secure.

## Require checks on GitHub

After the workflows have run, configure repository Rulesets / Branch protection to require the branch’s aggregate `CI result`, which rejects failures and unexpectedly skipped suites. YAML alone cannot enable branch protection. The repository includes configuration, not a claim that remote policies have been enabled.

## Local checks

Finish code and documentation before building:

```sh
python3 scripts/security/check_repository_hygiene.py workflows
python3 scripts/security/check_repository_hygiene.py secrets
python3 scripts/security/check_repository_hygiene.py paths
cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build-checked --parallel 4
```

For memory and undefined-behavior errors, build project code with ASan and UBSan:

```sh
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel 4
```

Run the sanitized build in a real or nested session to exercise the paths under review. A build alone does not verify physical GPU drivers or a complete standalone login session.

References: [CodeQL builds](https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages), [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html).


## UI/process safety notes in 1.0.1a

Settings hardware probes and portal path handling do not build shell command strings from user input. FileChooser location input accepts only validated local absolute paths or local `file://` URLs. System-tool launches come from fixed executable/argument allowlists.

Logout requires an explicit `quit confirm` control request. The compositor logs clean event-loop shutdown separately from fatal signals, making session-loss reports distinguish deliberate logout from a crash. Native plugins still execute in-process and remain trusted code.
