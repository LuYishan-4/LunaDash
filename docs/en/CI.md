# Continuous integration

CI only builds. Tests, contract checks and runtime sessions run locally with
`scripts/run-tests.sh` ([testing](TESTING.md)); run it before pushing.

`dev-ci.yml` and `main-ci.yml` are separate entry points sharing `main-build.yml`.

| Branch/event | Builds |
| --- | --- |
| `dev` push or PR | Ubuntu, website, Nix and Arch, selected from the complete Git diff |
| `main` push or PR | Every build, with all five distributions |
| Merge queue | Every build, with all five distributions |
| Weekly schedule | Every build on the default branch, `main` |
| Manual `Dev CI` | Select builds from the last commit; select `full` to build everything |

CI/policy changes and unknown source domains select every build and the full distribution
matrix, as do new branches and unavailable base commits. Renames and deletions take part in
selection. Documentation changes do not build the desktop on `dev`; website changes build the
website. `scripts/ci/changes.py` makes the selection (`tests/ci/test_changes.py` covers it).

## Builds

- **Ubuntu:** one cached CMake build (ccache, Slint's Cargo output, SlintLu) of the compositor,
  apps, SlintLu and plugin SDK, with `-DBUILD_TESTING=OFF`.
- **Website:** production build of the Astro site.
- **NixOS:** `nix build` of the x86_64 package. Slint and SlintLu are separate derivations and
  the Nix store is cached between runs, so only changed derivations rebuild; a cold store can
  take over an hour. See [NixOS](NIXOS.md).
- **Distributions:** Arch for development changes; Arch, Debian 13, Fedora 44, openSUSE
  Tumbleweed and Alpine Edge for full runs. Each installs its dependencies with
  `scripts/install-dependencies.sh` and builds the release configuration.

No workflow runs tests, linters or analyzers; `pr-gate.yml` only checks the pull-request policy.

Superseded runs on the same PR/branch are cancelled and jobs have time limits. Build failures
become annotations (`scripts/ci/report_build_errors.py`). `CI result` rejects failed or
cancelled builds and selected builds that were skipped; deliberately unselected builds are
allowed. Require this aggregate result for each branch in repository rulesets; workflow files do
not configure branch protection themselves.

Push selection compares against the most recent successful push of the same branch and
workflow, so a cancelled or failed run cannot hide earlier unbuilt changes. If GitHub cannot
provide that baseline, everything builds.

The contribution policy (`pr-gate.yml`) still targets `dev` and protects generated release
notes; workflow contributions are allowed for normal review. A same-repository `dev` → `main`
promotion is explicitly allowed, including previously reviewed workflow changes. Forks named
`dev` do not receive that exception. The policy is read from the PR base commit.

A green CI run proves that the commit builds, nothing more. Physical GPU/input, real login,
shell visuals and application compatibility still need release-session testing. See
[testing](TESTING_AND_FILES.md) and [security checks](SECURITY_CHECKS.md). Public Pages
deployment happens from `main`.
