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

## Release notes

GitHub Release Markdown is the canonical release-note source. Do not maintain a second hand-written changelog page for the same release.

When a GitHub Release is published or edited, `.github/workflows/release-notes.yml`:

1. Checks out `dev`.
2. Reads the release tag, title, date, URL, prerelease flag, and Markdown body from the GitHub release event payload.
3. Writes the Markdown body to `site/src/pages/releases/<tag>.md` with Astro frontmatter.
4. Updates `site/src/data/releases.json` used by the website release index.
5. Commits the generated website files back to `dev`.

The normal website workflow then checks and builds the updated release pages.

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

Keep the release body valid GitHub Markdown. Headings, lists, code fences, tables, links, and images supported by Astro Markdown can appear on the website release page.

## Branch flow

```text
feature/fix branch -> PR -> dev -> stabilization -> main -> release tag
```

`main` is the release branch. Ordinary feature/fix PRs do not target `main` directly.
