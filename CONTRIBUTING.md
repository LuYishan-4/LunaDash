# Contributing to LunaDash

LunaDash uses `dev` as its development branch. **Every pull request must target `dev`.** Do not open pull requests against `main` or any other branch. A pull request with another base branch will not be reviewed until it is retargeted to `dev`.

Our goal is a useful desktop from the first login, with room for personal customization. Contributions should make the default experience approachable or give users a clear, optional way to adapt it.

## Pull request titles

The title is the patch name. It must say what the change does and carry tags that match the change types checked in the description:

```
[Category] Specific summary of the change
```

Use `[Source]`, `[Script]` and `[Other]`, in that order, for every change type the PR contains:

- `[Source] Restore region screenshots after cancellation`
- `[Script] Report missing fixtures in check-source-layout`
- `[Other] Document NixOS packaging options`
- `[Source][Other] Match file chooser controls to the desktop theme`

The summary must describe the final change for someone who has not read the discussion. Titles such as `Dev`, `Update`, `Fix bugs` or `[Source] Fix` are not accepted. If later commits change what the PR does or which types it touches, update the title.

## Change type

Every PR description must check the change types that the diff actually contains, across all commits. Check every type that applies, and only those:

| Type | Covers |
| --- | --- |
| **Source** | Code that builds into or runs as part of LunaDash: C++, QML, shaders, CMake, plugins and modules, and their tests |
| **Script** | Helper and automation scripts: files under `scripts/` (such as `check-source-layout.py`) and their fixtures, and other shell, Python, packaging or install scripts |
| **Other** | Everything else: documentation, README translations, images and branding, CI workflows, packaging metadata such as `packaging/shell-revision`, and repository configuration. List the changed files in the PR template. |

Reviewers compare the checked types and the title tags with the full diff. If they do not match, for example a type is checked that the diff does not touch, or the diff touches a type that is not checked, the reviewer will ask you to correct the checkboxes and the title. Review continues, and the PR can be merged, only after they match. When you push commits that change the scope, update the checkboxes and the title yourself.

## AI assistance disclosure

Every PR must state honestly whether AI tools helped produce it. This covers chat assistants, coding agents that edit files, run commands or open pull requests, and AI code completion, whenever they generated or changed any part of the PR: code, scripts, tests, documentation, translations, commit messages, or the PR description itself. Conventional spell checkers, formatters and linters that do not generate content do not need to be listed.

If AI tools were used, name each tool, state which parts it generated or changed, and explain how you checked the result. If an agent pushed commits or opened the PR on your behalf, say so.

Using AI tools is not by itself a reason to reject a PR. You remain responsible for the whole change: read and understand every line, be ready to explain it in review, and list only validation you ran yourself, not results reported by a tool. If a reviewer finds that the disclosure is missing or inaccurate, you will be asked to correct it. Deliberate or repeated misreporting can lead to the PR being closed.

## Descriptions

Every PR must use the [PR template](.github/pull_request_template.md). Simple PRs can stay short: fill in the required sections and delete optional sections that do not apply. For larger changes, group related changes. Remove unused placeholders and do not mark checks complete unless they were performed. Report failed or skipped checks explicitly, including policy failures; a successful build does not mean every CI check passed.

Start the description with the problem and the observable result. Include:

- **Behavior:** what triggers the problem, what happened before, and what happens after the change.
- **Defaults and customization:** how the change improves the first-login experience or an optional setting, shortcut, module, or plugin. Omit this when unrelated.
- **Scope:** the affected features and source areas. Mention documentation or website changes only when they are actually part of the PR.
- **Validation:** commands or checks actually run, their results, and the platforms or hardware covered. For visual changes, include before/after screenshots when useful and check long translations and smaller windows.
- **Limitations and release impact:** behavior still unverified, required dependencies, configuration changes, and whether users must log in again or restart.

## Pull request requirements

Every pull request must:

1. Target the `dev` branch.
2. Use a title in the `[Category] Specific summary` format whose tags match the checked change types.
3. Check every change type (Source, Script, Other) that the diff contains, and only those.
4. State honestly whether AI tools were used and, if so, which tools and for what.
5. Keep the PR focused on the implementation being changed. Documentation and website updates are optional and should only be included when they are genuinely needed for that change.
6. Workflow changes are welcome and run the full CI suites. Keep generated website release pages and their index out of LunaDash-docs commits; deployments regenerate them from GitHub Releases.
7. Describe the observable behavior before and after the change.
8. List the validation actually run and any unverified platform or hardware scope.
9. Review security-sensitive input, process execution, file paths, permissions, QObject lifetimes, and Wayland client lifetime when applicable.

The `PR policy gate` workflow enforces the target branch: pull requests from forks and from branches in this repository must all target `dev`. Reviewers check the title, the change types and the AI disclosure. Dev CI selects shared suites from the complete diff; Main CI and full runs include every suite. Expensive analyzer builds run on full checks or CI/security changes. See [CI policy](docs/en/CI.md), [repository workflow](docs/en/COMPONENT_REPOS.md) and [NixOS packaging](docs/en/NIXOS.md).

## Branch flow

- Feature/fix branch -> pull request -> `dev`
- QML commit in LunaDash-Shell -> reviewed `packaging/shell-revision` update in LunaDash -> `dev`
- Website commit in LunaDash-docs -> website repository CI -> root Pages workflow fetches `main` on deployment
- `dev` -> stabilization / merge queue -> `main` (maintainers only)
- Release tags are created from the intended release commit.

`main` is the release branch, and contributors never target it. Maintainers handle promotion from `dev` to `main` and release integration separately; it is not a contribution pull request.

## Documentation and website

Documentation and website updates are **not mandatory for every pull request**. Add them when the change alters public behavior, user-facing configuration, installation steps, compatibility requirements, APIs, plugin contracts, or other information users need to understand.

Technical documentation belongs in `docs/en/` and its Traditional Chinese translation in `docs/zh/`. Keep matching filenames in both language directories when those documents are updated. Shared screenshots/branding stay under `docs/image/` and `docs/brand/`. Public-facing summaries, guides, API changes, screenshots, navigation, or release-facing copy belong in LunaDash-docs. Commit website changes there; the website is not checked out inside LunaDash and has no root revision pointer.

README translations live in `docs/readme/`: Traditional Chinese (`README.zh-TW.md`), Simplified Chinese (`README.zh-CN.md`) and Japanese (`README.ja.md`). When README content is changed, use the Traditional Chinese README as the content reference and synchronize the root English README, Simplified Chinese and Japanese editions with it.

Do not make empty, unrelated, or mechanical documentation/website edits just to satisfy CI.

## Release notes

Workflow maintenance is handled separately by the maintainer, outside normal contribution PRs.

GitHub Release Markdown is the source of truth for release notes. Describe release impact in the PR body; do not edit the generated website release Markdown or index. The Pages deployment workflow copies published release bodies into the Astro website under `/releases/<tag>/` and updates the release index. Release events trigger this workflow, but deployment must also satisfy the `github-pages` environment rules. If a release tag is blocked, maintainers can run the workflow from the permitted `main` branch. Verify the deployment and the live release page before reporting that the website is updated; do not bypass environment protections or hand-edit generated release files.

See `docs/en/RELEASE_PROCESS.md` for the release-note format and automation details.

## Native architecture

Follow [the source architecture](docs/en/ARCHITECTURE.md): lowercase domains, PascalCase C++ files, `LunaDash` namespace, small domain-local `Main.cpp`, and explicit CMake sources. Keep OpenGL implementation/resources under `compositor/renderer/opengl`. Run `python3 scripts/check-source-layout.py` and its fixture regressions before submitting. Preserve remote `dev` behavior when resolving a structural move.
