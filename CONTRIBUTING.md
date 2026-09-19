# Contributing to LunaDash

LunaDash uses `dev` as the integration branch. All normal pull requests must target `dev`; do not open feature or fix pull requests directly against `main`.

Our goal is a useful desktop from the first login, with room for personal customization. Contributions should make the default experience approachable or give users a clear, optional way to adapt it.

## Titles and descriptions

Use a specific title that describes the resulting change, such as `Restore screenshot selection after cancellation`. Start the description with the user-visible problem and result. Explain how the change affects the default experience, which preferences or modules users can customize, and any relevant limitations. For documentation changes, identify the information or workflow that becomes clearer.

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

`main` is the release branch. Do not bypass `dev` for ordinary contribution pull requests.

## Documentation and website

Documentation belongs in `docs/`. Public-facing summaries, guides, API changes, screenshots, navigation, or release-facing copy belong in `site/`.

README translations live in `docs/readme/`: Traditional Chinese (`README.zh-TW.md`), Simplified Chinese (`README.zh-CN.md`) and Japanese (`README.ja.md`). Keep their installation instructions, compatibility limits and contribution rules aligned with the root English README. Reuse the shared images and distribution badges; maintain the language-switch links in every version.

If a change does not need a large documentation rewrite, add a concise note to the closest existing document and update the matching website copy or guide. Do not make empty or unrelated edits only to satisfy CI.

## Release notes

Workflow maintenance is handled separately by the maintainer, outside normal contribution PRs.

GitHub Release Markdown is the source of truth for release notes. Describe release impact in the PR body; do not edit the generated website release Markdown or index. When a GitHub Release is published or edited, `.github/workflows/site-pages.yml` copies the release body into the Astro website under `/releases/<tag>/` and updates the release index.

See `docs/RELEASE_PROCESS.md` for the release-note format and automation details.

## Native architecture

Follow [the source architecture](docs/ARCHITECTURE.md): lowercase domains, PascalCase C++ files, `LunaDash` namespace, small domain-local `Main.cpp`, and explicit CMake sources. Keep OpenGL implementation/resources under `compositor/renderer/opengl`. Run `python3 scripts/check-source-layout.py` and its fixture regressions before submitting. Finish related docs, site and packaging changes before building; preserve remote `dev` behavior when resolving a structural move.
