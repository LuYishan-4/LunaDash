# Security and crash checks

Core build/runtime workflows run on pushes, pull requests, merge groups and manual dispatch. Source architecture also runs on `dev`/`main` pushes. Expensive PR analysis is path-scoped; inspect the exact workflow and commit result instead of assuming a push ran every PR-only analyzer. Pull-request code executes on GitHub-hosted runners, never through `pull_request_target`; checkout does not retain credentials. Only CodeQL has `security-events: write`. The website deployment workflow is the only one with `pages: write` and `id-token: write`; it is path-filtered to website changes and runs on `main` pushes and manual dispatch, never on pull requests.

| Check | Coverage | Failure condition |
| --- | --- | --- |
| Ubuntu build | Ubuntu 24.04 C/C++ build, file tests and architecture checks | Nonzero build/test exit |
| Source architecture | Naming, dependency direction, GL ownership and explicit source/resource inventory | Any architecture violation |
| Workflow policy and style | Workflow structure, permissions, credential persistence and unsafe triggers | Policy or style violation |
| Credential and personal path scan | API keys, tokens, private keys and user-specific filesystem paths in source/configuration | Any high-confidence match |
| Source language and shell style | English source policy, shell syntax and shellcheck errors | Any violation |
| C/C++ static analysis | Null/dangling pointers, suspicious memory operations, use-after-move, security APIs | Any enabled clang-tidy warning in project/test source |
| CodeQL security and quality | C/C++ dataflow, security and quality queries | SARIF error or security-severity finding; quality warnings remain annotations; missing reports also fail |
| Graphics diagnostics | Qt shader selection and graphics pipeline creation logs | Missing GLSL variant or failed pipeline, including sessions that exit with zero |

CodeQL builds a real CMake database including Qt/moc. A completed analysis is not the same as no findings: `scripts/security/check_sarif.py` blocks security-tagged findings and explicit SARIF errors without printing source snippets. Non-security quality warnings remain visible in CodeQL annotations but do not by themselves fail the pull request. Public repositories or appropriately licensed private repositories are required for GitHub code scanning. Setup/licensing failures remain visible failures. The CodeQL job also grants `actions: read`, which its workflow-run lookup needs in private repositories (see the [official workflow template](https://github.com/actions/starter-workflows/blob/main/code-scanning/codeql.yml)). Exported SARIF files are checked even when the analysis upload step fails; the failed upload still fails the job.

Main OpenGL tests the separately built renderer with software Mesa and a staged test installation; wlroots session tests select headless pixman. Dynamic checks cover only executed paths; static analysis is not a proof that the application is secure.

Ubuntu 24.04 static analysis uses the distribution's Clang and clang-tidy packages. The workflow first runs `tests/security/test_analyzer.py`, which requires both a valid Qt guard to pass and an intentionally invalid lifetime to fail with `clang-analyzer-cplusplus.NewDelete`. Diagnostics are line-filtered to the synthetic project source so a Clang 18 diagnostic whose primary location is only inside Qt system headers cannot make the valid `QPointer` guard fail; the analyzer check itself stays enabled.

## Require checks on GitHub

After the workflows have run, configure repository Rulesets / Branch protection to require the current build/runtime jobs plus the website, architecture and relevant PR analysis jobs. YAML alone cannot enable branch protection. The repository includes configuration, not a claim that remote policies have been enabled.

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

The command above checks a headless wlroots pixman session. For the separate Xvfb/Mesa OpenGL regression, use the commands in [Graphics](GRAPHICS.md). Neither verifies physical GPU drivers or a complete standalone login session.

References: [CodeQL builds](https://docs.github.com/en/code-security/reference/code-scanning/codeql/build-options-for-compiled-languages), [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html).


## UI/process safety notes in 1.0.1a

Settings hardware probes and portal path handling do not build shell command strings from user input. FileChooser location input accepts only validated local absolute paths or local `file://` URLs. System-tool launches come from fixed executable/argument allowlists.

Logout requires an explicit `quit confirm` control request. The compositor logs clean event-loop shutdown separately from fatal signals, making session-loss reports distinguish deliberate logout from a crash. Native plugins still execute in-process and remain trusted code.
