# 網站與 GitHub Pages

Astro 網站原始碼只放在 [LunaDash-docs](https://github.com/LunaDashWM/LunaDash-docs)，不會被 checkout 到 LunaDash 根 repository。LunaDash-docs 自己執行 website CI；根 Pages workflow 發布時，才會在 GitHub Actions runner clone 該 repo 的 `main` branch。

Home page 介紹 desktop、客製化、截圖、安裝與貢獻方式。開場動畫支援 reduced motion。網站不載入 analytics 或外部 font service；作者 avatar 由作者的主機提供。只有使用者按下 Copy 時才會操作 clipboard。

## 截圖與 badge

四張原始截圖放在 LunaDash `docs/image/`，並複製到 LunaDash-docs `docs/image/` 供獨立 build。`ScreenshotCard.astro` 使用網站副本並透過 Astro image pipeline 產生響應式 WebP preview。原圖連結不需要 JavaScript；網站另提供支援鍵盤的圖片 dialog，具有 Escape、關閉按鈕與焦點還原。

更換圖片時，同步調整 caption 和 alternative text。截圖應呈現真實 desktop 行為，並分別展示 dashboard、appearance、grouped windows 與 calendar。截圖日期代表拍攝時間，不代表 release 日期或所有機器的相容性。

README 的 distribution badge 使用 Shields.io `flat-square` 風格和白色 logo。版本與覆蓋範圍應符合 dependency installer 和已設定的 CI。網站安裝區保持簡短並連到詳細安裝指南。CI badge 不代表已在實體硬體上驗證。

## 預覽與建置

Clone LunaDash-docs，並在該 repository 建置：

```sh
git clone https://github.com/LunaDashWM/LunaDash-docs.git
cd LunaDash-docs
npm ci --include=dev --ignore-scripts
python3 scripts/sync-slintlu.py ../SlintLu   # 選用：更新 SlintLu 頁面
npm run build
npm run preview -- --host 127.0.0.1
```

預覽時開啟 `http://localhost:4321/LunaDash-docs/`。`SITE_URL` 與 `BASE_PATH` 決定發布網址（預設 `https://lunadashwm.github.io` 與 `/LunaDash-docs`）。產生的 `dist/` 不 commit。

## 貢獻與發布

網站 commit 放在 LunaDash-docs。不要 commit `src/pages/releases/` 下產生的 Markdown／MDX 或 `src/data/releases.json`；部署時會由 GitHub Releases 產生。`/slintlu/` 頁面由 `scripts/sync-slintlu.py` 從 SlintLu repository 產生，請在 SlintLu 修改。

- LunaDash-docs 自己的 CI 建置 PR；`pages.yml` 在每次 push 到 `main`（以及每日）時，以最新的 SlintLu 文件與 LunaDash release notes 建置並部署到 `https://lunadashwm.github.io/LunaDash-docs/`。
- LunaDash 的 `site-pages.yml` 以相同方式建置同一份 source，部署到 `https://luyishan-4.github.io/LunaDash/`，在 LunaDash `main` push、每日排程、GitHub Release 事件或手動 dispatch 時執行。

兩個 repository 的 Pages publishing source 都必須設定為 GitHub Actions。回復網站請 revert 並重新部署，不要改寫共用歷史。

技術指南放在 LunaDash `docs/en/` 與 `docs/zh/`，和網站公開的 `/docs/` 路徑不同。Release notes 使用公開 `/releases/` 路徑。詳細技術內容保留在 source guide，讓首頁專注於 desktop 與安裝方式。
