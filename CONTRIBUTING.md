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
- **Validation:** the commands actually run, their results, and the platforms or hardware covered. For visible UI changes, include before/after screenshots from a real session and check smaller windows.
- **Limitations and release impact:** behavior still unverified, required dependencies, configuration changes, updated `shell-revision` or `slintlu-revision` pins, and whether users must log in again or restart.

## Before pushing

CI only builds. Dev CI selects builds from the complete diff, Main CI and full runs build everything, and no workflow runs linters or analyzers. A green CI run proves only that the commit builds. Run the checks that apply to your change locally before pushing:

```sh
python3 scripts/check-source-layout.py
python3 scripts/ci/check_shell_actions.py
python3 scripts/security/check_repository_hygiene.py workflows   # also: secrets, paths
cmake -S . -B build -G Ninja && cmake --build build --parallel
```

State in the PR which commands you ran and what they reported. These checks and a successful build do not replace a real session: physical GPU and input, login, shell visuals and application compatibility still need manual verification in a real session. See [build and source map](docs/en/TESTING_AND_FILES.md) and [CI policy](docs/en/CI.md).

## Pull request requirements

Every pull request must:

1. Target the `dev` branch.
2. Use a title in the `[Category] Specific summary` format whose tags match the checked change types.
3. Check every change type (Source, Script, Other) that the diff contains, and only those.
4. State honestly whether AI tools were used and, if so, which tools and for what.
5. Keep the PR focused on the implementation being changed. Documentation and website updates are optional and should only be included when they are genuinely needed for that change.
6. Workflow changes are welcome; CI and policy changes select every build and the full distribution matrix. Keep generated website release pages and their index out of LunaDash-docs commits; deployments regenerate them from GitHub Releases.
7. Describe the observable behavior before and after the change.
8. List the commands actually run with their results, and the platforms or hardware covered, including any unverified platform or hardware scope.
9. Review security-sensitive input, process execution (never build shell command strings from user input or evaluate a selected path through a shell), file paths, permissions, native plugin trust, and wlroots and Wayland object and listener lifetimes when applicable.

The `PR policy gate` (`.github/workflows/pr-gate.yml`, `scripts/ci/check_pr_policy.py`) runs from the PR base commit and inspects the diff without executing proposed code. It rejects contribution PRs whose base is not `dev` and changes to generated release notes. Only a same-repository `dev` → `main` promotion is exempt; a fork's branch named `dev` is not. Reviewers check the title, the change types and the AI disclosure. See [CI policy](docs/en/CI.md), [repository workflow](docs/en/COMPONENT_REPOS.md) and [NixOS packaging](docs/en/NIXOS.md).

## Branch flow

- Feature/fix branch -> pull request -> `dev`
- Slint shell commit on LunaDash-Shell `main` -> reviewed `packaging/shell-revision` update in LunaDash -> `dev`
- Shell runtime commit in SlintLu -> reviewed `packaging/slintlu-revision` update in LunaDash -> `dev`
- Website commit on LunaDash-docs `main` -> LunaDash-docs deploys itself; the root Pages workflow fetches that branch on deployment
- `dev` -> stabilization / merge queue -> `main`, through a same-repository promotion PR opened by maintainers
- Release tags are created from the intended release commit.

`main` is the release branch, and contributors never target it. Maintainers promote `dev` to `main` and handle release integration; before promoting, they run the checks above and a full build on the promotion commit. See [pull requests and releases](docs/en/RELEASE_PROCESS.md).


Run `python3 scripts/check-source-layout.py` after changing native source layout or build lists. Preserve remote `dev` behavior when resolving a structural move.
