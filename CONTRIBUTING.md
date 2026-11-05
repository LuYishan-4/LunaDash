# Contributing to LunaDash

LunaDash uses `dev` as its development branch. **Every contribution pull request must target `dev`.** Do not open pull requests against `main` or any other branch. A contribution PR with another base branch fails the `PR policy gate` and is not reviewed until it is retargeted to `dev`. 


## Pull request titles

The title is the patch name. It must say what the change does and carry tags that match the change types checked in the PR description:

```
[Category] Specific summary of the change
```

Use `[Source]`, `[Script]` and `[Other]`, in that order, for every change type the PR contains:

- `[Source] Fix compositor hang when parsing audio device names`
- `[Script] Report missing packages in install-dependencies.sh`
- `[Other] Pin LunaDash-Shell to the frosted overlay revision`
- `[Source][Other] Add video wallpapers and document the setting`

The summary must describe the final change for someone who has not read the discussion. Titles such as `Dev`, `Update`, `Fix bugs` or `[Source] Fix` are not accepted. If later commits change what the PR does or which types it touches, update the title.

## Change type

Every PR must check the change types that its diff contains, across all commits. Check every type that applies, and only those. The type is decided by the path of each changed file:

| Type | Paths |
| --- | --- |
| **Source** | `src/` (C++, C, Slint and shaders), `protocols/`, `CMakeLists.txt` and `cmake/` |
| **Script** | `scripts/` (including `scripts/ci/`, `scripts/security/`, `scripts/site/` and session helpers such as `lunadash-session`) and `install.sh` |
| **Other** | Everything else: `docs/`, `README.md`, `AGENTS.md`, `data/` (translations, themes, icons, wallpapers, session and portal files), `packaging/` (including `shell-revision` and `slintlu-revision`), `nix/`, `flake.nix`, `flake.lock`, `.github/`, and tool configuration such as `.clang-format`. List the changed files in the PR description. |

Files under `tests/` count as the type of the code they test. For example, `tests/ci/test_pr_policy.py` tests `scripts/ci/check_pr_policy.py`, so it counts as Script; `tests/renderer/` counts as Source.

Reviewers compare the checked types and the title tags with the full diff. If they do not match, for example a type is checked that the diff does not touch, or the diff touches a type that is not checked, the reviewer will ask you to correct the checkboxes and the title. Review continues, and the PR can be merged, only after they match. When you push commits that change the scope, update the checkboxes and the title yourself.

## AI assistance disclosure

Every PR must state honestly whether AI tools helped produce it. This covers chat assistants, coding agents that edit files, run commands or open pull requests, and AI code completion, whenever they generated or changed any part of the PR: code, scripts, tests, documentation, translations, commit messages, or the PR description itself. Conventional spell checkers, formatters and linters that do not generate content do not need to be listed.

If AI tools were used, name each tool, state which parts it generated or changed, and explain how you checked the result. If an agent created the branch, pushed commits or opened the PR on your behalf, say so.

Using AI tools is not by itself a reason to reject a PR. You remain responsible for the whole change: read and understand every line, be ready to explain it in review, and list only validation you ran yourself, not results reported by a tool. If a reviewer finds that the disclosure is missing or inaccurate, you will be asked to correct it. Deliberate or repeated misreporting can lead to the PR being closed.

## Descriptions

Every PR must use the [PR template](.github/pull_request_template.md) and answer every question in it. Simple PRs can answer briefly; for larger changes, group related changes. Do not mark checks complete unless they were performed. Report failed or skipped checks explicitly, including policy failures.

Start the description with the problem and the observable result. Include:

- **Behavior:** what triggers the problem, what happened before, and what happens after the change.
- **Defaults and customization:** how the change improves the first-login experience or an optional setting, shortcut, module, or plugin. Omit this when unrelated.
- **Scope:** the affected features and source areas. Mention documentation or website changes only when they are actually part of the PR.
- **Validation:** the `scripts/run-tests.sh` suites and other commands actually run, their results, and the platforms or hardware covered. For visible UI changes, include before/after screenshots from a real session and check long translations and smaller windows.
- **Limitations and release impact:** behavior still unverified, required dependencies, configuration changes, updated `shell-revision` or `slintlu-revision` pins, and whether users must log in again or restart.

## Testing

CI only builds. Dev CI selects builds from the complete diff, Main CI and full runs build everything, and no workflow runs tests, linters or analyzers. A green CI run proves only that the commit builds. Run the test suites locally before pushing:

```sh
./scripts/run-tests.sh              # contracts, CTest and the headless runtime sessions
./scripts/run-tests.sh contracts    # repository, source, packaging and translation checks; no build
./scripts/run-tests.sh all          # also the installer suites
```

The runner builds into `build-tests/`, keeps each check's log in `build-tests/test-logs/` and ends with a pass/fail summary. State in the PR which suites you ran and what the summary reported. Runtime sessions are headless and do not replace a real session: physical GPU and input, login, shell visuals and application compatibility still need testing in a real session. See [testing](docs/en/TESTING.md), [testing and source map](docs/en/TESTING_AND_FILES.md) and [CI policy](docs/en/CI.md).

## Pull request requirements

Every pull request must:

1. Target the `dev` branch.
2. Use a title in the `[Category] Specific summary` format whose tags match the checked change types.
3. Check every change type (Source, Script, Other) that the diff contains, and only those.
4. State honestly whether AI tools were used and, if so, which tools and for what.
5. Keep the PR focused on the implementation being changed. Documentation and website updates are optional and should only be included when they are genuinely needed for that change.
6. Workflow changes are welcome; CI and policy changes select every build and the full distribution matrix. Keep generated website release pages and their index out of LunaDash-docs commits; deployments regenerate them from GitHub Releases.
7. Describe the observable behavior before and after the change.
8. List the validation actually run, including the `scripts/run-tests.sh` suites, and any unverified platform or hardware scope.
9. Review security-sensitive input, process execution (never build shell command strings from user input or evaluate a selected path through a shell), file paths, permissions, native plugin trust, and wlroots and Wayland object and listener lifetimes when applicable.

The `PR policy gate` (`.github/workflows/pr-gate.yml`, `scripts/ci/check_pr_policy.py`) runs from the PR base commit and inspects the diff without executing proposed code. It rejects contribution PRs whose base is not `dev` and changes to generated release notes. Only a same-repository `dev` → `main` promotion is exempt; a fork's branch named `dev` is not. Reviewers check the title, the change types and the AI disclosure. See [CI policy](docs/en/CI.md), [repository workflow](docs/en/COMPONENT_REPOS.md) and [NixOS packaging](docs/en/NIXOS.md).

## Branch flow

- Feature/fix branch -> pull request -> `dev`
- Slint shell commit on LunaDash-Shell `main` -> reviewed `packaging/shell-revision` update in LunaDash -> `dev`
- Shell runtime commit in SlintLu -> reviewed `packaging/slintlu-revision` update in LunaDash -> `dev`
- Website commit on LunaDash-docs `main` -> LunaDash-docs deploys itself; the root Pages workflow fetches that branch on deployment
- `dev` -> stabilization / merge queue -> `main`, through a same-repository promotion PR opened by maintainers
- Release tags are created from the intended release commit.

`main` is the release branch, and contributors never target it. Maintainers promote `dev` to `main` and handle release integration; before promoting, they run `./scripts/run-tests.sh all` on the promotion commit. See [pull requests and releases](docs/en/RELEASE_PROCESS.md).

## Documentation and website

Documentation and website updates are **not mandatory for every pull request**. Add them when the change alters public behavior, user-facing configuration, installation steps, compatibility requirements, APIs, plugin contracts, or other information users need to understand.

Technical documentation belongs in `docs/en/` and its Traditional Chinese translation in `docs/zh/`. Keep matching filenames in both language directories when those documents are updated. Root `docs/*.md` files stay small English forwarding pages. `docs/en/`, root documentation and the PR template must stay English; `tests/security/test_source_language.py` checks this. Shared screenshots/branding stay under `docs/image/` and `docs/brand/`. Public-facing summaries, guides, API changes, screenshots, navigation, or release-facing copy belong in LunaDash-docs. Commit website changes there; the website is not checked out inside LunaDash and has no root revision pointer.

README translations live in `docs/readme/`: Traditional Chinese (`README.zh-TW.md`), Simplified Chinese (`README.zh-CN.md`) and Japanese (`README.ja.md`). When README content is changed, use the Traditional Chinese README as the content reference and synchronize the root English README, Simplified Chinese and Japanese editions with it.

Do not make empty, unrelated, or mechanical documentation/website edits just to satisfy CI.

## Release notes

GitHub Release Markdown is the source of truth for release notes. Describe release impact in the PR body; do not edit the generated website release Markdown or index. The Pages deployment workflow (`.github/workflows/site-pages.yml`) runs `scripts/site/sync-releases.py`, writes each published release body as `LunaDash-docs/src/pages/releases/<tag>.md` and regenerates `LunaDash-docs/src/data/releases.json` in its deployment workspace. Release events trigger this workflow, but deployment must also satisfy the `github-pages` environment rules. If a release tag is blocked, maintainers can run the workflow from the permitted `main` branch. Verify the deployment and the live release page before reporting that the website is updated; do not bypass environment protections or hand-edit generated release files.

See [pull requests and releases](docs/en/RELEASE_PROCESS.md) for the release-note format and automation details.

## Native architecture

Follow [the source architecture](docs/en/ARCHITECTURE.md) and [`AGENTS.md`](AGENTS.md):

- C++20 with a C11 rendering core. C core functions use the `ludash_` prefix, and all project C++ types and functions live in the `LunaDash` namespace (except `main`).
- Lowercase source domains, PascalCase `.hpp`/`.cpp` files, source-root project includes, small domain-local `Main.cpp` entry points, and every source listed explicitly in CMake.
- The compositor and built-in desktop services use wlroots and standard Wayland protocols and do not link Qt. LunaDash-owned built-in UI uses Slint.
- Raw OpenGL code and built-in shaders stay under `src/compositor/renderer/opengl/`; shaders are embedded through CMake.
- Shell behavior changes belong in LunaDash-Shell and missing shell runtime features in SlintLu; then update the pinned revision. The former Quickshell/QML shell is no longer supported.
- Source text stays English; UI translations belong in `data/translations/`.
- Native plugins stay disabled by default. Plugin metadata is validation, not a sandbox.

Run `python3 scripts/check-source-layout.py` after changing native source layout or build lists; `./scripts/run-tests.sh contracts` runs it together with its regression tests in `tests/architecture/test_source_layout.py`. Preserve remote `dev` behavior when resolving a structural move.
