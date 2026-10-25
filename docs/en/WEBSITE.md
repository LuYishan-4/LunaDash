# Website and GitHub Pages

The English Astro site source lives only in [LunaDash-docs](https://github.com/LunaDashWM/LunaDash-docs). LunaDash does not check it out under the root repository. LunaDash-docs owns its website CI; the root Pages workflow clones its current `main` branch in the GitHub Actions runner when publishing.

The home page presents the desktop, customization options, screenshots, installation and contribution guidance. The short opening animation respects reduced motion. There are no analytics or font services. The contributor avatar loads from the maintainer's host. Clipboard access occurs only when Copy is clicked.

## Screenshots and badges

The four original screenshots are maintained in LunaDash `docs/image/` and copied into LunaDash-docs `docs/image/` for its independent build. `ScreenshotCard.astro` imports the website copies and uses Astro's image pipeline for responsive WebP previews. Full-size links work without JavaScript; the website additionally offers a keyboard-accessible image dialog with Escape, a close button and focus restoration.

Update each image's caption and alternative text when replacing it. Show real desktop behavior and keep examples distinct: dashboard, appearance, grouped windows and calendar. The screenshot dates describe when the images were captured, not a release date or a claim about every machine.

Distribution badges belong in README, using Shields.io's `flat-square` style and white logos. Keep their versions and coverage labels aligned with the dependency installer and configured CI. The website keeps its installation section short and links to the installation guide for distribution details. CI labels do not establish physical hardware compatibility.

## Preview and validation

Clone LunaDash-docs and run its checks from that repository:

```sh
git clone https://github.com/LunaDashWM/LunaDash-docs.git
cd LunaDash-docs
npm ci --include=dev --ignore-scripts
npm run check
npm run build
python3 scripts/test-site.py dist
npm run preview -- --host 127.0.0.1
```

Open `http://localhost:4321/LunaDash/` while the preview server is running. Check desktop and mobile layouts, all four images, the image dialog, keyboard focus, copy feedback and documentation links. Generated output under `dist/` is ignored. LunaDash keeps the shared brand banner at `docs/brand/banner.svg`; the website keeps its matching copy at `public/assets/banner.svg`. The website CI checks the built copy against its source asset.

## Contributions and publishing

Website commits belong in LunaDash-docs and run through its own CI. Keep generated release Markdown (`.md` or `.mdx`) under `src/pages/releases/` and the generated `src/data/releases.json` index out of repository commits. Release impact belongs in the LunaDash PR body; deployments generate these files from GitHub Releases.

- `main-site.yml` in LunaDash fetches LunaDash-docs `main`, checks types, builds the site and validates links/assets.
- `site-pages.yml` in LunaDash fetches the same source, synchronizes published Release Markdown, repeats validation and deploys `dist/` to GitHub Pages. It runs after pushes to LunaDash `main`, on a daily schedule, for GitHub Releases and when manually dispatched.

A successful website build on LunaDash `dev` does not publish the website. After the intended revision reaches LunaDash-docs `main`, inspect its CI and the next Pages run. Pages refreshes on the next LunaDash `main` push or daily run; maintainers can also dispatch the root workflow manually. The configured address is `https://luyishan-4.github.io/LunaDash/`; Pages must use GitHub Actions as its publishing source. Update both `site` and `base` in LunaDash-docs `astro.config.mjs` if the repository URL changes. Roll back with a revert and redeployment rather than rewriting shared history.

Repository technical guides live under LunaDash `docs/en/` and `docs/zh/`; those paths differ from the website's public `/docs/` routes. Release notes live under the public `/releases/` route. Keep technical detail in the source guides so the home page can stay focused on the desktop and how to install it.
