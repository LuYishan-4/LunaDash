# Website and GitHub Pages

The English introduction site lives in `site/`. It uses HTML, CSS and strict TypeScript, compiled to browser JavaScript, relative asset URLs, local images and no external analytics or font services. The accent and gap controls change the website preview only; they do not connect to a desktop session. Clipboard access occurs only when Copy is clicked.

Preview from the repository root:

```sh
npm ci --prefix site --ignore-scripts
npm run check --prefix site
npm run build --prefix site
python3 tests/site/test_site.py site/dist
python3 -m http.server 8080 --bind 127.0.0.1 --directory site/dist
```

Open `http://127.0.0.1:8080`. Check desktop and mobile widths, keyboard focus, accent buttons, gap slider, copy feedback and documentation links. The hero image is a real LuDash screenshot captured with user/hostname display disabled.

## Publishing

The intended project URL is `https://luyishan-4.github.io/LuDash/`. A URL in this document does not by itself confirm deployment.

`.github/workflows/pages.yml` type-checks TypeScript and validates compiled site assets on matching pull requests. Main-branch pushes deploy only after validation; pull requests never deploy. Repository Settings → Pages must use **GitHub Actions** as the source. The deployment job has `pages: write` and `id-token: write`, uses the `github-pages` environment and uploads only the validated `site/dist/` artifact. Enable Pages with an authorized account if the repository has not used it before.

After changing site files, push the reviewed changes to main or run **Documentation website** manually. Inspect the Actions result and load the published URL to confirm the deployment. For a rollback, revert the relevant website change and let the workflow redeploy; do not force-push shared history.

The website links to the English Markdown guides in the main branch. The source tree includes the entire per-file map and testing instructions. The pinned TypeScript compiler is a development dependency; browsers load no framework runtime. `site/src/app.ts` is the interaction source, and `site/dist/` is ignored generated output. No user configuration, logs, credentials or native binaries belong in `site/`.

Reference: [GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).
