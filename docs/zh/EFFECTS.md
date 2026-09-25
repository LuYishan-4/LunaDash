# 視窗動畫與 Renderer Effects

[English](../en/EFFECTS.md) · [繁中索引](README.md)

活動中的 wlroots compositor 使用 `src/compositor/window/animation/SceneAnimationBackend`。預設 220 ms：map 時淡入並輕微上移；close/unmap 時保留 scene snapshot，讓 surface 已消失後動畫仍能正常結束。Move 會更新 scene position。

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl appearance '{"animations":true,"animationDuration":220}'
./build/lunadashctl appearance '{"animations":false}'
```

`effects.activeAnimations` 會回報活動 transition。關閉動畫或 duration=0 可作 reduced motion。Close 不會繞過 application 本身的 save/cancel dialog。

## 應用程式毛玻璃

既有 wlroots scene 現在會在一般 Wayland 與 XWayland 應用程式視窗後方繪製模糊背景。效果依視窗角色套用，不使用應用程式白名單，因此原生、GTK、Qt、Electron 與 XWayland 視窗走相同路徑。應用程式文字與控制項在模糊背景上方合成，本身不會被模糊。不透明程式必須將「視窗不透明度」降至 100% 以下才會看見背景；新設定檔預設 90%，既有使用者儲存值仍優先。

在「設定 > 視覺效果」調整「背景模糊」、「模糊強度」（0–32）與「視窗不透明度」（60–100%）。關閉模糊或將強度設為零會移除背景效果。全螢幕與護眼模式維持不透明。程式若本身提供透明背景，可保留 100% 視窗不透明度，仍能看到後方模糊。降低整體視窗不透明度也會讓文字較透明；這項設定不會辨識或取代各程式的背景顏色。

`renderer/blur/SceneBackdrop.c` 透過活動中的 wlroots renderer 擷取視窗下方的場景，包含下方重疊視窗，排除視窗本身與其前一次模糊背景，再執行水平、垂直各九個取樣的 Gaussian pass。擷取採降解析度，每軸限制 1024 像素，先保留邊緣取樣空間再裁切。沒有將畫面讀回 CPU，也沒有替換 renderer。GLES 沿用既有 renderer；pixman 可在 CI 驗證相同合成路徑。`WindowGlass` 管理各視窗的背景節點、重用未變更的擷取，並遵循 scene node 生命週期。原生特效插件仍預設停用。

狀態中的 `blurReady`、`blurFailed`、`blurFrames` 與 blur error 反映實際結果。buffer 無法匯入或配置 render target 失敗時，程式仍能使用並回報錯誤。這是開發中的特效，不代表所有硬體驅動、HDR／色彩管理路徑、影片 overlay 或應用程式均已驗證。Layer-shell 面板與特殊全螢幕表面不屬於一般應用程式視窗效果範圍。

在 wlroots 0.19 以上版本，若需要取樣的下方來源視窗使用 explicit sync，該次模糊背景會停用。在非同步 commit-release 資源管理完成前，這項可選特效不會排入額外的來源讀取。使用 explicit sync 的目標視窗仍可顯示由下方 implicit-sync 內容組成的模糊背景，因為目標本身不會被取樣。一般應用程式畫面仍由 wlroots 正常繪製。

獨立 Qt render-element library 仍保留 `renderer/opengl` 下的 OpenGL blur、procedural wallpaper 與 decoration shader，沒有取代活動中的 wlroots scene。CI 包含背景隔離、Gaussian 模糊像素、尺寸界線與資源生命週期測試；Arch shell workflow 會擷取實際軟體 Wayland 工作階段。最終視覺驗證仍需實體 GPU 效能測試與真實登入工作階段截圖。

Quickshell 自己也會動畫 shell surface 與 wallpaper transition。GPU 效能與 application 相容性仍需真實 session 驗證。詳見 [Graphics](GRAPHICS.md) 與 [Architecture](ARCHITECTURE.md)。

## NyxNiri 桌面整合、Orbit 與動態桌布

新增模組、色盤與 portal 服務、快捷鍵、相依套件及驗證界線，請參閱[桌面整合說明](NYXNIRI_DESKTOP.md)。
