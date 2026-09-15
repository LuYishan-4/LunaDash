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

The default port is 4321; open `http://localhost:4321/LunaDash/` while the local server is running. For the configured project base, use `npm run dev --prefix site -- --host 127.0.0.1` and open the printed `/LunaDash/` URL. Check desktop and mobile widths, keyboard focus, the boot animation, copy feedback and documentation links. Reduced motion skips the boot animation. `site/public/assets/banner.svg` is a copy of `docs/brand/banner.svg`, and `tests/site/test_site.py` fails if the two ever differ.

## Publishing

The intended project URL is `https://luyishan-4.github.io/LunaDash/`, the Pages address for the `LuYishan-4/LunaDash` repository. A URL in this document does not by itself confirm deployment.

Two workflows cover the website, and both are configured in this repository:

- `.github/workflows/main-site.yml` checks types, builds the site and runs the site test on main pushes, pull requests, merge groups and manual runs. It holds read-only `contents` permission.
- `.github/workflows/site-pages.yml` is the dedicated deployment workflow. It repeats the check, build and test, then uploads and deploys `site/dist` to GitHub Pages on main pushes that touch the website and on manual runs. `pages: write` and `id-token: write` are granted only to this workflow, and its `pages` concurrency group serialises deployments instead of cancelling them.

Neither workflow has been observed to run from this environment; a green local build is not evidence that a Pages job ran.

1. Set **Settings → Pages → Build and deployment → Source** to **GitHub Actions** before expecting a deployment.
2. Push the website to `main`, or run the deployment workflow manually, then open the deployment URL reported by the `github-pages` environment.
3. Check the home page and all four guide routes under `/LunaDash/`.

Website and documentation files cannot travel through a pull request: `.github/workflows/pr-documentation-scope.yml` rejects a pull request that modifies `docs/`, `site/` or Markdown. Website changes are therefore committed to `main`, which makes the main-push run of `main-site.yml` the only automated validation point.

The `site` and `base` values in `site/astro.config.mjs` already match `https://luyishan-4.github.io/LunaDash/`; update both when changing the account, repository name or domain. GitHub Free supports Pages for public repositories; private repositories require an eligible plan. See [GitHub's publishing-source instructions](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site).

For a rollback, revert the website change and redeploy; do not force-push shared history.

The site includes four English guide routes: `/docs/settings/`, `/docs/modules/`, `/docs/api/` and `/docs/start/`. They cover every settings category, a local-only JSON style playground, QML contracts, native plugin metadata and bounded Unix-socket examples. Markdown source guides remain linked for deeper detail. The source tree includes the entire per-file map and testing instructions. Astro generates the static browser HTML during build; no handwritten `.html` page is maintained. Astro and the TypeScript checker are pinned development dependencies. `site/src/app.ts` is the interaction source, and `site/dist/` is ignored generated output. No user configuration, logs, credentials or native binaries belong in `site/`.

Reference: [GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

The source page is `site/src/pages/index.astro`; `site/astro.config.mjs` configures the GitHub Pages base. Setting the Pages source to GitHub Actions and approving the `github-pages` environment are repository-owner actions outside the source tree.
