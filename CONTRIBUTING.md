# Contributing to LunaDash

LunaDash uses `dev` as the integration branch. All normal pull requests must target `dev`; do not open feature or fix pull requests directly against `main`.

Our goal is a useful desktop from the first login, with room for personal customization. Contributions should make the default experience approachable or give users a clear, optional way to adapt it.

## Titles and descriptions

Use a specific title that describes the resulting change, such as `Restore screenshot selection after cancellation`. Start the description with the user-visible problem and result. Explain how the change affects the default experience, which preferences or modules users can customize, and any relevant limitations. For documentation changes, identify the information or workflow that becomes clearer.

## Pull request requirements

Every pull request must:

1. Target the `dev` branch.
2. Update at least one file under `docs/` to document the behavior, contract, configuration, testing, or limitation that changed.
3. Update at least one file under `site/` so the public website stays aligned with the product and documentation.
4. Describe the observable behavior before and after the change.
5. List the validation actually run and any unverified platform or hardware scope.
6. Review security-sensitive input, process execution, file paths, permissions, QObject lifetimes, and Wayland client lifetime when applicable.

The `PR policy gate` workflow enforces the target branch and the `docs/` + `site/` update rule. Source, security, and lifetime checks are path-scoped so documentation-only changes do not run C/C++ analysis unnecessarily.

## Branch flow

- Feature/fix branch -> pull request -> `dev`
- `dev` -> stabilization / merge queue -> `main`
- Release tags are created from the intended release commit.

`main` is the release branch. Do not bypass `dev` for ordinary contribution pull requests.

## Documentation and website

Documentation belongs in `docs/`. Public-facing summaries, guides, API changes, screenshots, navigation, or release-facing copy belong in `site/`.

If a change does not need a large documentation rewrite, add a concise note to the closest existing document and update the matching website copy or guide. Do not make empty or unrelated edits only to satisfy CI.

## Release notes

GitHub Release Markdown is the source of truth for release notes. When a GitHub Release is published or edited, `.github/workflows/site-pages.yml` copies the release body into the Astro website under `/releases/<tag>/` and updates the release index.

See `docs/RELEASE_PROCESS.md` for the release-note format and automation details.

## Native architecture

Follow [the source architecture](docs/ARCHITECTURE.md): lowercase domains, PascalCase C++ files, `LunaDash` namespace, small domain-local `Main.cpp`, and explicit CMake sources. Keep OpenGL implementation/resources under `compositor/renderer/opengl`. Run `python3 scripts/check-source-layout.py` and its fixture regressions before submitting. Finish related docs, site and packaging changes before building; preserve remote `dev` behavior when resolving a structural move.
