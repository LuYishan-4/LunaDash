# Pull requests and releases

## Pull requests

Normal pull requests must target `dev`. The PR policy gate rejects pull requests opened against another base branch.

Every pull request must update both:

- `docs/` — the technical or user-facing documentation for the changed behavior.
- `site/` — the public website copy, guide, API page, or other matching public content.

This is intentionally strict so the repository documentation and public website do not drift behind implementation work.

PR CI is split by cost:

- **PR policy gate** always checks the base branch and required documentation/site updates.
- **Repository hygiene** runs for every PR targeting `dev`.
- **Source/QML style** runs only when source, QML, scripts, tests, or build files change.
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
