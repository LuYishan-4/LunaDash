# Contributing to LunaDash

LunaDash uses `dev` as the integration branch. All normal pull requests must target `dev`; do not open feature or fix pull requests directly against `main`.

Our goal is a useful desktop from the first login, with room for personal customization. Contributions should make the default experience approachable or give users a clear, optional way to adapt it.

## Titles and descriptions

Describe the final change for someone who has not read the discussion. Use a specific title such as `Restore region screenshots after cancellation` or `Match file chooser controls to the desktop theme`; avoid titles such as `Dev`, `Update`, or `Fix bugs`.

Start the description with the problem and the observable result. Include:

- **Behavior:** what triggers the problem, what happened before, and what happens after the change.
- **Defaults and customization:** how the change improves the first-login experience or an optional setting, shortcut, module, or plugin. Omit this when unrelated.
- **Scope:** the affected features, documentation and website pages. Only include changes present in the PR; identify later commits separately if they matter.
- **Validation:** commands or checks actually run, their results, and the platforms or hardware covered. For visual changes, include before/after screenshots when useful and check long translations and smaller windows.
- **Limitations and release impact:** behavior still unverified, required dependencies, configuration changes, and whether users must log in again or restart.

Keep simple PRs short. For larger changes, use the [PR template](.github/pull_request_template.md) and group related changes. Remove unused placeholders and do not mark checks complete unless they were performed. Report failed or skipped checks explicitly, including policy failures; a successful build does not mean every CI check passed.

## Pull request requirements

Every pull request must:

1. Target the `dev` branch.
2. Update at least one file under `docs/` to document the behavior, contract, configuration, testing, or limitation that changed.
3. Update matching public content under `site/`, such as a guide or feature description. Generated release notes do not count.
4. Do not add, edit, delete or rename files under `.github/workflows/`, release Markdown under `site/src/pages/releases/` (`.md` or `.mdx`), or `site/src/data/releases.json`.
5. Describe the observable behavior before and after the change.
6. List the validation actually run and any unverified platform or hardware scope.
7. Review security-sensitive input, process execution, file paths, permissions, QObject lifetimes, and Wayland client lifetime when applicable.

The `PR policy gate` workflow enforces the target branch, the `docs/` + `site/` update rule, and the protected paths above. It runs the policy script from the base commit and inspects the proposed diff without checking out or running PR code. Source, security, and lifetime checks are path-scoped so documentation-only changes do not run C/C++ analysis unnecessarily.

## Branch flow

- Feature/fix branch -> pull request -> `dev`
- `dev` -> stabilization / merge queue -> `main`
- Release tags are created from the intended release commit.

`main` is the release branch. Do not bypass `dev` for ordinary contribution pull requests. Maintainers handle release integration separately; the current PR policy gate requires `dev` as the target and does not exempt `dev → main` release PRs.

## Documentation and website

Technical documentation belongs in `docs/en/` and its Traditional Chinese translation in `docs/zh/`. Keep matching filenames in both language directories and update both when behavior changes. Shared screenshots/branding stay under `docs/image/` and `docs/brand/`. Public-facing summaries, guides, API changes, screenshots, navigation, or release-facing copy belong in `site/`.

README translations live in `docs/readme/`: Traditional Chinese (`README.zh-TW.md`), Simplified Chinese (`README.zh-CN.md`) and Japanese (`README.ja.md`). Use the Traditional Chinese README as the content reference, and synchronize the root English README, Simplified Chinese and Japanese editions with it. Keep the same sections, feature descriptions, installation instructions, compatibility statements and contribution rules in each language. Reuse the shared images and distribution badges; maintain the language-switch links in every version.

If a change does not need a large documentation rewrite, add a concise note to the closest existing document and update the matching website copy or guide. Do not make empty or unrelated edits only to satisfy CI.

## Release notes

Workflow maintenance is handled separately by the maintainer, outside normal contribution PRs.

GitHub Release Markdown is the source of truth for release notes. Describe release impact in the PR body; do not edit the generated website release Markdown or index. The Pages deployment workflow copies published release bodies into the Astro website under `/releases/<tag>/` and updates the release index. Release events trigger this workflow, but deployment must also satisfy the `github-pages` environment rules. If a release tag is blocked, maintainers can run the workflow from the permitted `main` branch. Verify the deployment and the live release page before reporting that the website is updated; do not bypass environment protections or hand-edit generated release files.

See `docs/en/RELEASE_PROCESS.md` for the release-note format and automation details.

## Native architecture

Follow [the source architecture](docs/en/ARCHITECTURE.md): lowercase domains, PascalCase C++ files, `LunaDash` namespace, small domain-local `Main.cpp`, and explicit CMake sources. Keep OpenGL implementation/resources under `compositor/renderer/opengl`. Run `python3 scripts/check-source-layout.py` and its fixture regressions before submitting. Finish related docs, site and packaging changes before building; preserve remote `dev` behavior when resolving a structural move.
