# Website and GitHub Pages

The English Astro site source lives only in [LunaDash-docs](https://github.com/LunaDashWM/LunaDash-docs). LunaDash does not check it out under the root repository. LunaDash-docs owns its website CI; the root Pages workflow clones its current `main` branch in the GitHub Actions runner when publishing.

The home page presents the desktop, customization options, screenshots, installation and contribution guidance. The short opening animation respects reduced motion. There are no analytics or font services. The contributor avatar loads from the maintainer's host. Clipboard access occurs only when Copy is clicked.

## Screenshots and badges

The four original screenshots are maintained in LunaDash `docs/image/` and copied into LunaDash-docs `docs/image/` for its independent build. `ScreenshotCard.astro` imports the website copies and uses Astro's image pipeline for responsive WebP previews. Full-size links work without JavaScript; the website additionally offers a keyboard-accessible image dialog with Escape, a close button and focus restoration.

Update each image's caption and alternative text when replacing it. Show real desktop behavior and keep examples distinct: dashboard, appearance, grouped windows and calendar. The screenshot dates describe when the images were captured, not a release date or a claim about every machine.

Distribution badges belong in README, using Shields.io's `flat-square` style and white logos. Keep their versions and coverage labels aligned with the dependency installer and configured CI. The website keeps its installation section short and links to the installation guide for distribution details. CI labels do not establish physical hardware compatibility.

## Preview and build

Clone LunaDash-docs and build it from that repository:

```sh
git clone https://github.com/LunaDashWM/LunaDash-docs.git
cd LunaDash-docs
npm ci --include=dev --ignore-scripts
python3 scripts/sync-slintlu.py ../SlintLu   # optional: refresh the SlintLu pages
npm run build
npm run preview -- --host 127.0.0.1
```

Open `http://localhost:4321/LunaDash-docs/` while the preview server is running. `SITE_URL` and `BASE_PATH` choose the published address (default `https://lunadashwm.github.io` and `/LunaDash-docs`). Generated output under `dist/` is ignored.

## Contributions and publishing

Website commits belong in LunaDash-docs. Keep generated release Markdown (`.md` or `.mdx`) under `src/pages/releases/` and the generated `src/data/releases.json` index out of repository commits; deployments generate them from GitHub Releases. The `/slintlu/` pages are generated from the SlintLu repository by `scripts/sync-slintlu.py`; edit them there.

- LunaDash-docs builds pull requests in its own CI, and its `pages.yml` builds every push to `main` (and runs daily) with the current SlintLu documentation and LunaDash release notes, then deploys to `https://lunadashwm.github.io/LunaDash-docs/`.
- `site-pages.yml` in LunaDash builds the same source the same way for `https://luyishan-4.github.io/LunaDash/`, after pushes to LunaDash `main`, daily, for GitHub Releases and when dispatched.

Both repositories need GitHub Actions as their Pages publishing source. Roll back with a revert and redeployment rather than rewriting shared history.

Repository technical guides live under LunaDash `docs/en/` and `docs/zh/`; those paths differ from the website's public `/docs/` routes. Release notes live under the public `/releases/` route. Keep technical detail in the source guides so the home page can stay focused on the desktop and how to install it.
