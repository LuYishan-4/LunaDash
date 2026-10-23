# 網站與 GitHub Pages

Astro 網站原始碼只放在 [LunaDash-docs](https://github.com/LunaDash-DE/LunaDash-docs)，不會被 checkout 到 LunaDash 根 repository。LunaDash-docs 自己執行 website CI；根 Pages workflow 發布時，才會在 GitHub Actions runner clone 該 repo 的 `main` branch。

Home page 介紹 desktop、客製化、截圖、安裝與貢獻方式。開場動畫支援 reduced motion。網站不載入 analytics 或外部 font service；作者 avatar 由作者的主機提供。只有使用者按下 Copy 時才會操作 clipboard。

## 截圖與 badge

四張原始截圖放在 LunaDash `docs/image/`，並複製到 LunaDash-docs `docs/image/` 供獨立 build。`ScreenshotCard.astro` 使用網站副本並透過 Astro image pipeline 產生響應式 WebP preview。原圖連結不需要 JavaScript；網站另提供支援鍵盤的圖片 dialog，具有 Escape、關閉按鈕與焦點還原。

更換圖片時，同步調整 caption 和 alternative text。截圖應呈現真實 desktop 行為，並分別展示 dashboard、appearance、grouped windows 與 calendar。截圖日期代表拍攝時間，不代表 release 日期或所有機器的相容性。

README 的 distribution badge 使用 Shields.io `flat-square` 風格和白色 logo。版本與覆蓋範圍應符合 dependency installer 和已設定的 CI。網站安裝區保持簡短並連到詳細安裝指南。CI badge 不代表已在實體硬體上驗證。

## 預覽與驗證

Clone LunaDash-docs，並在該 repository 執行：

```sh
git clone https://github.com/LunaDash-DE/LunaDash-docs.git
cd LunaDash-docs
npm ci --include=dev --ignore-scripts
npm run check
npm run build
python3 scripts/test-site.py dist
npm run preview -- --host 127.0.0.1
```

預覽時開啟 `http://localhost:4321/LunaDash/`，檢查 desktop/mobile、四張圖片、dialog、keyboard focus、copy feedback 和文件連結。產生的 `dist/` 不 commit。共用 banner 原始檔留在 LunaDash `docs/brand/banner.svg`；網站 repo 的 `public/assets/banner.svg` 保留相同副本，website CI 會檢查 build 後的檔案。

## 貢獻與發布

網站 commit 放在 LunaDash-docs 並通過其 CI。不要 commit `src/pages/releases/` 下產生的 Markdown／MDX 或產生的 `src/data/releases.json`。Release impact 寫在 LunaDash PR body；部署時會由 GitHub Releases 產生網站 release notes。

- LunaDash 的 `main-site.yml` 抓取 LunaDash-docs `main`，檢查型別、build 網站並驗證連結和資產。
- LunaDash 的 `site-pages.yml` 抓取同一個 source，同步已發布的 Release Markdown、重做驗證並部署 `dist/` 到 GitHub Pages。它會在 LunaDash `main` push、每日排程、GitHub Release 事件或手動 dispatch 時執行。

LunaDash `dev` 上的網站 build 不會發布網站。LunaDash-docs 的變更進入 `main` 後，請查看其 CI 與下一次 Pages workflow。Pages 會在下次 LunaDash `main` push 或每日排程更新；maintainer 也可手動 dispatch 根 repo workflow。公開網址為 `https://luyishan-4.github.io/LunaDash/`，Pages publishing source 必須設定為 GitHub Actions。若 repository URL 改變，請一起檢查 LunaDash-docs `astro.config.mjs` 的 `site` 和 `base`。回復網站請 revert 並重新部署，不要改寫共用歷史。

技術指南放在 LunaDash `docs/en/` 與 `docs/zh/`，和網站公開的 `/docs/` 路徑不同。Release notes 使用公開 `/releases/` 路徑。詳細技術內容保留在 source guide，讓首頁專注於 desktop 與安裝方式。
