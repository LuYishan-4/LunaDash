# Website and GitHub Pages

The English site lives in `site/` and uses Astro, CSS and TypeScript. Its introduction follows LunaDash's goal: a useful desktop from the first login, with room for customization. README and website installation examples use `./scripts/install-session.sh`; manual builds belong in the technical guides.

The home page presents the desktop, its customization options, screenshots, installation, distribution coverage and contribution guidance. The short opening animation respects reduced motion. There are no analytics or font services. Distribution badges load from Shields.io, and the contributor avatar loads from the maintainer's host. Clipboard access occurs only when Copy is clicked.

## Screenshots and badges

The four original screenshots are maintained in `docs/image/` and referenced by both README and the website. `ScreenshotCard.astro` imports those originals and uses Astro's image pipeline for responsive WebP previews. Full-size links work without JavaScript; the website additionally offers a keyboard-accessible image dialog with Escape, a close button and focus restoration.

Update each image's caption and alternative text when replacing it. Show real desktop behavior and keep examples distinct: dashboard, appearance, grouped windows and calendar. The screenshot dates describe when the images were captured, not a release date or a claim about every machine.

`site/src/data/distributions.ts` maintains the website's distribution badges and coverage labels. README uses the same Shields.io `flat-square` style, white distribution logos and matching versions. Keep both lists aligned with the dependency installer and configured CI. Ubuntu 24.04 has main build/runtime checks; Arch, Debian 13, Fedora 45, openSUSE Tumbleweed and Alpine Edge have distribution source-build jobs. Void and Gentoo have installer paths outside that matrix. These labels do not establish physical hardware compatibility.

## Preview and validation

From the repository root:

```sh
npm ci --prefix site --include=dev --ignore-scripts
npm run check --prefix site
npm run build --prefix site
python3 tests/site/test_site.py site/dist
npm run preview --prefix site -- --host 127.0.0.1
```

Open `http://localhost:4321/LunaDash/` while the preview server is running. Check desktop and mobile layouts, all four images, the image dialog, keyboard focus, copy feedback and documentation links. Generated output under `site/dist/` is ignored. The shared brand banner remains in `docs/brand/banner.svg` and `site/public/assets/banner.svg`; the site checks keep the copies aligned. The navigation icon uses the same crescent mark as `docs/brand/icon.svg`.

## Contributions and publishing

Normal pull requests target `dev` and include relevant `docs/` and `site/` changes. Describe the visible result, the effect on defaults or optional customization, and the validation actually performed. See [CONTRIBUTING.md](../CONTRIBUTING.md) and the [PR template](../.github/pull_request_template.md).

- `main-site.yml` checks types, builds and validates local links/assets on pushes, pull requests, merge groups and manual runs.
- `site-pages.yml` deploys from `main` when website, brand or screenshot assets change, when a GitHub Release changes, or when manually dispatched. It synchronizes published Release Markdown, repeats validation and deploys `site/dist` to GitHub Pages.

A successful build on `dev` does not publish the website. After the intended revision reaches `main`, inspect the Pages workflow and the URL reported by its deployment. The configured address is `https://luyishan-4.github.io/LunaDash/`; Pages must use GitHub Actions as its publishing source. Update both `site` and `base` in `site/astro.config.mjs` if the repository URL changes. Roll back with a revert and redeployment rather than rewriting shared history.

Source guides live under `/docs/`, with release notes under `/releases/`. Keep technical detail in those guides so the home page can stay focused on the desktop and how to install it.
