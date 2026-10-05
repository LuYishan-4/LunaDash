# Continuous integration

`dev-ci.yml` and `main-ci.yml` are separate entry points sharing `main-build.yml`. A feature test belongs in the maintained suite, not in another workflow.

| Branch/event | Suites |
| --- | --- |
| `dev` push or PR | Repository/source contracts always; affected Ubuntu runtime, QML, website, Nix and Arch suites selected from the complete Git diff |
| `main` push or PR | All shared suites, all five distribution builds, clang-tidy/Qt lifetime and CodeQL |
| Merge queue | All suites and all five distribution builds |
| Weekly schedule | Full checks on the default branch, `main` |
| Manual `Dev CI` | Select suites from the last commit; select `full` to run everything |

CI/policy changes and unknown source domains enable every suite and the full distribution matrix. New branches or unavailable base commits also enable every suite. Renames and deletions participate in selection. Documentation changes avoid desktop builds on `dev`; website changes run Astro check/build/link tests. `main` remains complete even for documentation-only changes. `tests/ci/test_changes.py` protects these boundaries.

## Shared suites

- **Repository and source contracts:** source layout, English source, shell syntax/ShellCheck, QML actions/design, translations, installer/source archive contracts and repository hygiene. Independent steps continue after another contract step fails.
- **Ubuntu build and runtime:** one cached CMake build, CTest including real portal frontend tests, plugin SDK installation, compositor startup/linkage, Wayland/XWayland lifecycle and installed software OpenGL tests. Runtime suites reuse the build and independently report failures.
- **QML:** parse every shell file and run the entire Qt Quick test directory.
- **Website:** Astro type checking, production build, asset/link checks.
- **NixOS:** pinned package, module evaluation and installed headless runtime. x86_64 builds; aarch64 package evaluation only. See [NixOS](NIXOS.md).
- **Distributions:** Arch for development changes; Arch, Debian 13, Fedora 44, openSUSE Tumbleweed and Alpine Edge for full runs.
- **Security analysis:** Qt lifetime fixtures and clang-tidy production build, plus CodeQL and a SARIF security gate. Full runs and CI/security configuration changes enable these; ordinary development source pushes do not repeat two extra analyzer builds.

Superseded runs on the same PR/branch are cancelled. Jobs have time limits and retain runtime evidence. `CI result` rejects failed/cancelled suites and selected suites that were skipped; deliberately unselected suites are allowed. Require this aggregate result for each branch in repository rulesets; workflow files do not configure branch protection themselves.

The contribution policy still targets `dev` and protects generated release notes; workflow contributions are allowed for normal review. A same-repository `dev` → `main` promotion is explicitly allowed, including previously reviewed workflow changes. Forks named `dev` do not receive that exception. The policy is read from the PR base commit.

## Script maintenance and evidence

Removed unused `scripts/security/check_pr_scope.py` (the obsolete rule forbidding documentation), `scripts/security/run_local_audit.sh` (a duplicate audit/build wrapper), and `scripts/test-xdpw-sources.sh` (an unreferenced standalone probe). Active installer/session scripts, SDK helpers, diagnostics and documented manual tools remain.

A green build proves only the checks that ran at that commit. Physical GPU/input, real login, Quickshell visuals and application compatibility still need release-session testing. See [testing](TESTING_AND_FILES.md) and [security checks](SECURITY_CHECKS.md). Website source changes on `dev` are validated there; public Pages deployment happens from `main`.

The translation gate covers every shipped locale. Existing missing desktop strings and duplicate catalog keys were corrected when enabling the shared gate.

Runtime tests wait for document-portal FUSE teardown before removing their temporary directory. The notification lifecycle test allows extra time for the first lazy XWayland/GLX startup while retaining all 25 teardown and mapping checks.

GTK 3/4 and Qt lifecycle tests now run in Ubuntu CI and retain screenshot evidence. Fork CodeQL analysis still runs and gates SARIF, but does not attempt a security-events upload with a read-only fork token. Fedora uses stable 44 rather than the 45 beta; see the [Fedora release page](https://fedoraproject.org/).

The Arch distribution build reuses its compiled binaries for the first-login Quickshell test: capture Welcome, activate Start desktop with a Wayland virtual keyboard, and verify setup persistence after restarting. Distribution artifacts retain that software-session screenshot and runtime logs.
