# Pull requests and releases

## Pull requests

Normal pull requests must target `dev`. The PR policy gate rejects pull requests opened against another base branch.

Every pull request must update both:

- `docs/` — the technical or user-facing documentation for the changed behavior.
- `site/` — the public website copy, guide, API page, or other matching public content.

PRs must not add, modify, delete or rename GitHub workflow files under `.github/workflows/`, release Markdown (`.md` or `.mdx`) under `site/src/pages/releases/`, or `site/src/data/releases.json`. Workflow maintenance is handled separately by the maintainer. Generated release notes do not satisfy the website-update requirement: update relevant guides or product copy instead, and describe release impact in the PR body.

The policy script runs from the base commit, inspecting both sides of renames without checking out or executing PR code. This keeps changes to the proposed policy script from bypassing the check.

PR CI is split by cost:

- **PR policy gate** checks the base branch, required documentation/site updates, and protected workflow/release paths.
- **Repository hygiene** runs for every PR targeting `dev`.
- **Source/QML style** checks source architecture on `dev`/`main` pushes and on source-scoped PRs.
- **Qt lifetime**, **Clang-Tidy**, and **CodeQL** run only when C/C++ or build-system code changes.
- **Website build** runs for PRs targeting `dev` because every PR must include a `site/` update.

The main required-check gate is reserved for `main`/merge-queue validation instead of polling source-analysis workflows that may legitimately be skipped by path filters.

## Release notes

GitHub Release Markdown is the canonical release-note source. Do not maintain a second hand-written changelog page for the same release.

The Pages deployment workflow handles release notes without committing generated files back into `dev` or `main`:

1. A GitHub Release is published, edited, or deleted.
2. `.github/workflows/site-pages.yml` checks out the latest `main` website source.
3. `scripts/site/sync-releases.py` reads all non-draft releases through the GitHub Releases API.
4. Every Release body is written into the deployment workspace as `site/src/pages/releases/<tag>.md` with Astro frontmatter.
5. `site/src/data/releases.json` is regenerated for the `/releases/` index.
6. Astro checks/builds the site and GitHub Pages deploys the resulting release pages.

This means GitHub remains the source of truth: editing the GitHub Release Markdown changes the website on the next release-triggered deployment, and deleting a Release removes it from the next generated release index/page set. Generated Markdown is not committed to the repository.

The public routes are:

```text
/releases/                 all published releases
/releases/<tag>/           one release body rendered as Markdown
```

### Recommended GitHub Release Markdown

```markdown
## Highlights

- Major user-visible change.
- Another important improvement.

## Desktop and shell

- Detail the UI, compositor, window-management, or settings changes.

## Fixes

- Describe important bugs fixed in this version.

## Upgrade notes

- Mention configuration migrations, restart requirements, or known limitations.
```

Keep the release body valid GitHub Markdown. Headings, lists, code fences, tables, links and images supported by Astro Markdown can appear on the website release page.

## Branch flow

```text
feature/fix branch -> PR -> dev -> stabilization -> main -> release tag
```

`main` is the release branch. Ordinary feature/fix PRs do not target `main` directly.

## Architecture migration release checks

Before promoting the refactor, inspect every workflow run for its exact commit SHA, including website, source architecture and the distribution matrix. Build a source archive and stage a normal CMake installation; verify executables, compatibility aliases, session/portal entries, QML and translations. Built-in shaders are embedded from `renderer/opengl/shaders` and must not require a checkout after installation. The optional `Tests` install component exercises the relocated renderer executable and is not part of normal packages.

Release notes should mention the `LunaDash` C++ namespace, colocated PascalCase interfaces and domain-local entrypoints for contributors/plugin authors. Existing user settings and executable names remain compatible. Keep native effects disabled by default and rebuild external native plugins against the current header. Publish only validation actually performed: software OpenGL/pixman and distribution builds do not establish physical GPU, input-seat, Chrome/Zed, multiple-output or display-manager support. The architecture refactor does not turn the development desktop into a production-ready KDE replacement.
