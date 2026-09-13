# Security and crash checks

The Linux and security workflows run on pull requests, pushes, merge groups and manual dispatch. Core checks have no path filter. Pull-request code executes on GitHub-hosted runners, never through `pull_request_target`; checkout does not retain credentials. Only CodeQL has `security-events: write`. The separate Pages deployment grants Pages permissions only to the main-branch deployment job.

| Check | Coverage | Failure condition |
| --- | --- | --- |
| Linux build | Arch, Ubuntu and Fedora backend builds and tests | Nonzero build or test exit |
| C/C++ static analysis | Null/dangling pointers, suspicious memory operations, use-after-move, security APIs | Any enabled clang-tidy warning |
| ASan and UBSan | Executed out-of-bounds, use-after-free, double-free and undefined behavior paths | Sanitizer report or test failure |
| CodeQL security and quality | C/C++ dataflow, security and quality queries | SARIF error/warning or security-severity finding; missing reports also fail |
| Source language | C/C++/QML, documentation and website text policy | Chinese source text outside translation packs |
| Website validation | Assets, fragment targets, English language and image descriptions | Invalid local links or missing assets |

CodeQL builds a real CMake database including Qt/moc. A completed analysis is not the same as no findings: `scripts/security/check_sarif.py` makes reported findings fail the job without printing source snippets. Public repositories or appropriately licensed private repositories are required for GitHub code scanning. Setup/licensing failures remain visible failures.

The Ubuntu sanitizer job tests native client lifetimes with `--no-shell`; the Arch job additionally tests Quickshell, first-run setup and GL/GLES integration. Leak detection is disabled because Qt/Mesa retain process-wide allocations. **This does not test memory leaks.** Other ASan checks remain active. Dynamic checks cover only executed paths; static analysis is not a proof that the application is secure.

## Require checks on GitHub

After the workflows have run, configure repository Rulesets / Branch protection to require the three Linux build matrix checks, **C/C++ static analysis**, **ASan and UBSan**, and **CodeQL security and quality**. YAML alone cannot enable branch protection. The repository includes configuration, not a claim that remote policies have been enabled.

## Local checks

Finish code and documentation before building:

```sh
cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-checked --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  QT_QPA_PLATFORM=xcb LIBGL_ALWAYS_SOFTWARE=1 \
  xvfb-run -a ctest --test-dir build-checked --output-on-failure
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  LUDASH_BUILD_DIR="$PWD/build-checked" ./scripts/test-wayland.sh
```

The crash-regression test deliberately signals only its own child with core dumps disabled and requires compositor exit code 2. Never run this by targeting unrelated desktop processes. Configuration tests reject mixed valid/invalid updates before writing; network tests distinguish link state from verified Internet connectivity without changing host network settings.

References: [CodeQL builds](https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages), [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html).
