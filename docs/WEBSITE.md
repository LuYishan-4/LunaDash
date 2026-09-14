# Website and GitHub Pages

The English introduction site lives in `site/`. It uses Astro components, CSS and strict TypeScript, base-aware asset URLs, local images and no external analytics or font services. The accent and gap controls change the website preview only; they do not connect to a desktop session. Clipboard access occurs only when Copy is clicked.

Preview from the repository root:

```sh
npm ci --prefix site --include=dev --ignore-scripts
npm run check --prefix site
npm run build --prefix site
python3 tests/site/test_site.py site/dist
npm run preview --prefix site -- --host 127.0.0.1
```

The default port is 4321; open `http://localhost:4321/LuDash/` while the local server is running. For the configured project base, use `npm run dev --prefix site -- --host 127.0.0.1` and open the printed `/LuDash/` URL. Check desktop and mobile widths, keyboard focus, accent buttons, gap slider, copy feedback and documentation links. The hero image is a real LunaDah screenshot captured with user/hostname display disabled.

## Publishing

The intended project URL is `https://luyishan-4.github.io/LuDash/`. A URL in this document does not by itself confirm deployment.

`.github/workflows/pages.yml` type-checks TypeScript and validates compiled site assets on matching pull requests. Main and codex feature-branch pushes run validation; only main deploys after validation; pull requests never deploy. Repository Settings → Pages must use **GitHub Actions** as the source. The deployment job has `pages: write` and `id-token: write`, uses the `github-pages` environment and uploads only the validated `site/dist/` artifact. Enable Pages with an authorized account if the repository has not used it before.

After changing site files, push the reviewed changes to main or run **Documentation website** manually. Inspect the Actions result and load the published URL to confirm the deployment. For a rollback, revert the relevant website change and let the workflow redeploy; do not force-push shared history.

First deployment:

1. Open the LunaDah repository on GitHub, then **Settings → Pages → Build and deployment**.
2. Set **Source** to **GitHub Actions**. The repository already contains the workflow; no additional starter template is needed.
3. Push the website and workflow to `main`. To deploy an existing main revision, open **Actions → Documentation website → Run workflow**, select `main`, and run it.
4. Wait for both `validate` and `deploy` to succeed. Open the deployment URL shown by the `github-pages` environment.
5. Check the home page, Settings guide and API guide, including images and navigation under `/LuDash/`.

The push trigger watches `site/**`, `tests/site/**` and the Pages workflow. A C++-only push does not redeploy the website. The current `site` and `base` values in `site/astro.config.mjs` already match `https://luyishan-4.github.io/LuDash/`; update both when changing the account, repository name or domain. GitHub Free supports Pages for public repositories; private repositories require an eligible plan. See [GitHub's publishing-source instructions](https://docs.github.com/en/pages/getting-started-with-github-pages/configuring-a-publishing-source-for-your-github-pages-site).

The site includes four English guide routes: `/docs/settings/`, `/docs/modules/`, `/docs/api/` and `/docs/start/`. They cover every settings category, a local-only JSON style playground, QML contracts, native plugin metadata and bounded Unix-socket examples. Markdown source guides remain linked for deeper detail. The source tree includes the entire per-file map and testing instructions. Astro generates the static browser HTML during build; no handwritten `.html` page is maintained. Astro and the TypeScript checker are pinned development dependencies. `site/src/app.ts` is the interaction source, and `site/dist/` is ignored generated output. No user configuration, logs, credentials or native binaries belong in `site/`.

Reference: [GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

Deployment is deferred while DE work is prioritized. The source page is `site/src/pages/index.astro`; `site/astro.config.mjs` configures the GitHub Pages base.
