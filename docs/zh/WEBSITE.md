# Website 與 GitHub Pages

[English](../en/WEBSITE.md) · [繁中索引](README.md)

公開網站位於 `site/`，使用 Astro、CSS 與 TypeScript。Home page 介紹 LunaDash、客製化、截圖、安裝與貢獻；複雜技術細節應留在 source docs。網站不載入 analytics/font service，clipboard 只在使用者按 Copy 時操作。

## Screenshots / brand

四張實際桌面截圖共用 `docs/image/`。README 直接引用原圖；網站的 `ScreenshotCard.astro` 由 Astro image pipeline 產生 responsive WebP preview，full-size link 不需 JavaScript。

替換 screenshot 時同步更新 caption/alt text，並維持內容差異：dashboard、appearance、window/workspace、calendar。圖片日期表示 capture date，不等於 release date。

品牌原始檔：

```text
docs/brand/banner.svg
docs/brand/icon.svg
site/public/assets/banner.svg
site/public/assets/mark.svg
```

網站測試會檢查共用 brand copy 是否一致。

## 本機 preview

```sh
npm ci --prefix site --include=dev --ignore-scripts
npm run check --prefix site
npm run build --prefix site
python3 tests/site/test_site.py site/dist
npm run preview --prefix site -- --host 127.0.0.1
```

Preview：`http://localhost:4321/LunaDash/`。要檢查 desktop/mobile、四張圖、dialog、keyboard focus、copy feedback、documentation links。產生的 `site/dist/` 不 commit。

## 文件連結

新的 GitHub source 技術文件連結應使用：

```text
docs/en/<NAME>.md
docs/zh/<NAME>.md
```

不要再新增指向 root `docs/<NAME>.md` 的 source link。網站自己的公開 route `/docs/start/`、`/docs/settings/` 等是 Astro route，和 repo 內語言資料夾是不同概念，不應改成 `/docs/en/`。

## PR 與發布

一般 PR target `dev`，同步更新 `docs/en/`、`docs/zh/` 與 matching `site/`。Release Markdown 由 GitHub Releases 產生，不手動維護 generated release files。

- `main-site.yml`：網站 type/build/link/asset check。
- `site-pages.yml`：`main` 的網站/brand/screenshot 變更與 release event 後部署。

`dev` build 成功不會自動發布網站；預定 revision 進 `main` 後要查看 Pages workflow 與實際 deployment URL。

公開站址是 `https://luyishan-4.github.io/LunaDash/`。若 repo/site path 改變，要同步更新 `site/astro.config.mjs` 的 `site` / `base`。
