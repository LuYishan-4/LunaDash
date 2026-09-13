# Website and GitHub Pages

The English introduction site lives in `site/`. It uses plain HTML, CSS and JavaScript, relative asset URLs, local images and no external analytics or font services. The accent and gap controls change the website preview only; they do not connect to a desktop session. Clipboard access occurs only when Copy is clicked.

Preview from the repository root:

```sh
python3 tests/site/test_site.py
node --check site/app.js
python3 -m http.server 8080 --bind 127.0.0.1 --directory site
```

Open `http://127.0.0.1:8080`. Check desktop and mobile widths, keyboard focus, accent buttons, gap slider, copy feedback and documentation links. The hero image is a real LuDash screenshot captured with user/hostname display disabled.

## Publishing

The intended project URL is `https://luyishan-4.github.io/LuDash/`. A URL in this document does not by itself confirm deployment.

`.github/workflows/pages.yml` validates site assets and JavaScript on matching pull requests. Main-branch pushes deploy only after validation; pull requests never deploy. Repository Settings → Pages must use **GitHub Actions** as the source. The deployment job has `pages: write` and `id-token: write`, uses the `github-pages` environment and uploads only `site/`. Enable Pages with an authorized account if the repository has not used it before.

After changing site files, push the reviewed changes to main or run **Documentation website** manually. Inspect the Actions result and load the published URL to confirm the deployment. For a rollback, revert the relevant website change and let the workflow redeploy; do not force-push shared history.

The website links to the English Markdown guides in the main branch. The source tree includes the entire per-file map and testing instructions. No user configuration, logs, credentials or native binaries belong in `site/`.

Reference: [GitHub custom Pages workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).
