# Security and crash checks

The Linux and security workflows run on pull requests, pushes, merge groups and manual dispatch. Core checks have no path filter. Pull-request code executes on GitHub-hosted runners, never through `pull_request_target`; checkout does not retain credentials. Only CodeQL has `security-events: write`. The separate Pages deployment grants Pages permissions only to the main-branch deployment job.

| Check | Coverage | Failure condition |
| --- | --- | --- |
| Linux build | Arch, Ubuntu and Fedora backend builds and tests | Nonzero build or test exit |
| C/C++ static analysis | Null/dangling pointers, suspicious memory operations, use-after-move, security APIs | Any enabled clang-tidy warning |
| ASan and UBSan | Executed out-of-bounds, use-after-free, double-free and undefined behavior paths | Sanitizer report or test failure |
| CodeQL security and quality | C/C++ dataflow, security and quality queries | SARIF error/warning or security-severity finding; missing reports also fail |
| Source language | C/C++/QML, documentation and website text policy | Chinese source text outside translation packs |
| Graphics diagnostics | Qt shader selection and graphics pipeline creation logs | Missing GLSL variant or failed pipeline, including sessions that exit with zero |
| Rendering resources | Sustained compositor/shell descriptor and GPU fence counts | Unbounded growth beyond the test tolerance, pipe exhaustion or abnormal shutdown |
| Website validation | Assets, fragment targets, English language and image descriptions | Invalid local links or missing assets |

CodeQL builds a real CMake database including Qt/moc. A completed analysis is not the same as no findings: `scripts/security/check_sarif.py` makes reported findings fail the job without printing source snippets. Public repositories or appropriately licensed private repositories are required for GitHub code scanning. Setup/licensing failures remain visible failures. The CodeQL job also grants `actions: read`, which its workflow-run lookup needs in private repositories (see the [official workflow template](https://github.com/actions/starter-workflows/blob/main/code-scanning/codeql.yml)). Exported SARIF files are checked even when the analysis upload step fails; the failed upload still fails the job.

The Ubuntu sanitizer job tests native client lifetimes with `--no-shell`; the Arch job additionally tests Quickshell, first-run setup and GL/GLES integration. Leak detection is disabled because Qt/Mesa retain process-wide allocations. **This does not test memory leaks.** Other ASan checks remain active. Dynamic checks cover only executed paths; static analysis is not a proof that the application is secure.

Ubuntu 24.04 static analysis explicitly uses the distribution's `clang-19` and `clang-tidy-19` packages. Clang 18 misreports Qt 6.4's `QPointer` destruction inside `QWeakPointer`'s deallocator when Qt headers are system includes; a minimal valid guard reproduces the diagnostic without LunaDah code. Clang 19 accepts it. The workflow first runs `tests/security/test_analyzer.py`, which requires both a valid Qt guard to pass and an intentionally invalid lifetime to fail with `clang-analyzer-cplusplus.NewDelete`. No check is disabled and warnings remain errors. The sanitizer job independently uses Clang 18 and explicitly installs `libclang-rt-18-dev`.

## Require checks on GitHub

After the workflows have run, configure repository Rulesets / Branch protection to require the three Linux build matrix checks, **C/C++ static analysis**, **ASan and UBSan**, and **CodeQL security and quality**. YAML alone cannot enable branch protection. The repository includes configuration, not a claim that remote policies have been enabled.

## Local checks

Finish code and documentation before building:

```sh
python3 tests/security/test_analyzer.py clang-tidy
cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy -DLUDASH_ENABLE_SANITIZERS=ON
cmake --build build-checked --parallel 4
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  LUDASH_BUILD_DIR="$PWD/build-checked" ./scripts/test-wayland.sh
```

On Ubuntu 24.04, use `clang-19`, `clang++-19` and `clang-tidy-19` for the combined local command above, and install `libclang-rt-19-dev` for its sanitizer runtime. The separate CI sanitizer job stays on Clang 18 to verify the older compiler independently.

The crash-regression test deliberately signals only its own child with core dumps disabled and requires compositor exit code 2. Never run this by targeting unrelated desktop processes. Configuration tests reject mixed valid/invalid updates before writing; network tests distinguish link state from verified Internet connectivity without changing host network settings.

References: [CodeQL builds](https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages), [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html), [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html).
