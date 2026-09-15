# Website and GitHub Pages

The English site lives in `site/`. It uses Astro components, CSS and strict TypeScript, base-aware asset URLs and local images. The landing page opens with a short boot animation that mirrors the shell's startup overlay, then shows the README banner, Installation, Getting started, the source links and the contributors. It has no analytics, font services or cookies, and clipboard access occurs only when Copy is clicked. The single external request is the contributor avatar, which is loaded from the maintainer's own host.

Preview from the repository root:

```sh
npm ci --prefix site --include=dev --ignore-scripts
npm run check --prefix site
npm run build --prefix site
python3 tests/site/test_site.py site/dist
npm run preview --prefix site -- --host 127.0.0.1
```

The default port is 4321; open `http://localhost:4321/LuDash/` while the local server is running. For the configured project base, use `npm run dev --prefix site -- --host 127.0.0.1` and open the printed `/LuDash/` URL. Check desktop and mobile widths, keyboard focus, the boot animation, copy feedback and documentation links. Reduced motion skips the boot animation. `site/public/assets/banner.svg` is a copy of `docs/brand/banner.svg`, and `tests/site/test_site.py` fails if the two ever differ.

## Publishing

The intended project URL is `https://luyishan-4.github.io/LuDash/`. A URL in this document does not by itself confirm deployment.

The website sources remain under `site/`, but GitHub Pages automation is currently disabled. Build and validate the site locally before publishing it through a separately approved deployment process. If Pages is enabled later, use **GitHub Actions** as the source and grant deployment permissions only to the dedicated deployment workflow.

After changing site files, run the local validation commands below and inspect the generated site before publishing. For a rollback, revert the relevant website change; do not force-push shared history.

When deployment automation is restored:

1. Open the LunaDash repository on GitHub, then **Settings → Pages → Build and deployment**.
2. Set **Source** to **GitHub Actions** and add a dedicated, reviewed deployment workflow.
3. Push the website and deployment workflow to `main`.
4. Wait for validation and deployment to succeed. Open the deployment URL shown by the configured environment.
5. Check the home page, Settings guide and API guide, including images and navigation under `/LuDash/`.

There is currently no GitHub Actions trigger for website changes. The current `site` and `base` values in `site/astro.config.mjs` already match `https://luyishan-4.github.io/LuDash/`; update both when changing the account, repository name or domain. GitHub Free supports Pages for public repositories; private repositories require an eligible plan. See [GitHub's publishing-source instructions](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site).

The site includes four English guide routes: `/docs/settings/`, `/docs/modules/`, `/docs/api/` and `/docs/start/`. They cover every settings category, a local-only JSON style playground, QML contracts, native plugin metadata and bounded Unix-socket examples. Markdown source guides remain linked for deeper detail. The source tree includes the entire per-file map and testing instructions. Astro generates the static browser HTML during build; no handwritten `.html` page is maintained. Astro and the TypeScript checker are pinned development dependencies. `site/src/app.ts` is the interaction source, and `site/dist/` is ignored generated output. No user configuration, logs, credentials or native binaries belong in `site/`.

Reference: [GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

Deployment is deferred while DE work is prioritized. The source page is `site/src/pages/index.astro`; `site/astro.config.mjs` configures the GitHub Pages base.
