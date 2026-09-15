# Security and crash checks

The build and security workflows run on pull requests, pushes to `main`, merge groups and manual dispatch. Core checks have no path filter. Pull-request code executes on GitHub-hosted runners, never through `pull_request_target`; checkout does not retain credentials. Only CodeQL has `security-events: write`. The website deployment workflow is the only one with `pages: write` and `id-token: write`; it is path-filtered to website changes and runs on `main` pushes and manual dispatch, never on pull requests.

| Check | Coverage | Failure condition |
| --- | --- | --- |
| Ubuntu build | Ubuntu 24.04 backend build and desktop OpenGL smoke test | Nonzero build or graphics test exit |
| Workflow policy and style | Workflow structure, permissions, credential persistence and unsafe triggers | Policy or style violation |
| Credential and personal path scan | API keys, tokens, private keys and user-specific filesystem paths in source/configuration | Any high-confidence match |
| Source language and shell style | English source policy, shell syntax and shellcheck errors | Any violation |
| C/C++ static analysis | Null/dangling pointers, suspicious memory operations, use-after-move, security APIs | Any enabled clang-tidy warning |
| CodeQL security and quality | C/C++ dataflow, security and quality queries | SARIF error/warning or security-severity finding; missing reports also fail |
| Graphics diagnostics | Qt shader selection and graphics pipeline creation logs | Missing GLSL variant or failed pipeline, including sessions that exit with zero |

CodeQL builds a real CMake database including Qt/moc. A completed analysis is not the same as no findings: `scripts/security/check_sarif.py` makes reported findings fail the job without printing source snippets. Public repositories or appropriately licensed private repositories are required for GitHub code scanning. Setup/licensing failures remain visible failures. The CodeQL job also grants `actions: read`, which its workflow-run lookup needs in private repositories (see the [official workflow template](https://github.com/actions/starter-workflows/blob/main/code-scanning/codeql.yml)). Exported SARIF files are checked even when the analysis upload step fails; the failed upload still fails the job.

The Ubuntu build job tests desktop OpenGL loading with the software Mesa driver. Dynamic checks cover only executed paths; static analysis is not a proof that the application is secure.

Ubuntu 24.04 static analysis uses the distribution's Clang and clang-tidy packages. The workflow first runs `tests/security/test_analyzer.py`, which requires both a valid Qt guard to pass and an intentionally invalid lifetime to fail with `clang-analyzer-cplusplus.NewDelete`. No check is disabled and warnings remain errors.

## Require checks on GitHub

After the workflows have run, configure repository Rulesets / Branch protection to require **Ubuntu 24.04 build and OpenGL smoke test**, **C/C++ static analysis**, and **CodeQL security analysis**. YAML alone cannot enable branch protection. The repository includes configuration, not a claim that remote policies have been enabled.

## Local checks

Finish code and documentation before building:

```sh
python3 tests/security/test_analyzer.py clang-tidy
cmake -S . -B build-checked -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_CLANG_TIDY=clang-tidy
cmake --build build-checked --parallel 4
LUDASH_GRAPHICS=opengl LUDASH_TEST_NO_SHELL=1 \
  LUDASH_BUILD_DIR="$PWD/build-checked" ./scripts/test-wayland.sh
```

The graphics smoke test uses Xvfb/Mesa and does not verify physical GPU drivers or a complete standalone login session.

References: [CodeQL builds](https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages), [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html).
