# 視窗動畫與 Renderer Effects

[English](../en/EFFECTS.md) · [繁中索引](README.md)

活動中的 wlroots compositor 使用 `src/compositor/animation/SceneWindowAnimations`。預設 220 ms：map 時淡入並輕微上移；close/unmap 時保留 scene snapshot，讓 surface 已消失後動畫仍能正常結束。Move 會更新 scene position。

```sh
export LUDASH_CONTROL="$XDG_RUNTIME_DIR/ludash-test-control"
./build/lunadashctl appearance '{"animations":true,"animationDuration":220}'
./build/lunadashctl appearance '{"animations":false}'
```

`effects.activeAnimations` 會回報活動 transition。關閉動畫或 duration=0 可作 reduced motion。Close 不會繞過 application 本身的 save/cancel dialog。

Qt render-element library 另外保留 two-pass Gaussian blur、procedural wallpaper 與 decoration shader，位於 `src/compositor/renderer/opengl`；它是獨立建置與測試的 library，**目前沒有接到活動中的 wlroots scene**。因此單純打開 blur preference 不能當作 wlroots background blur 已實際渲染的證據，status 目前仍會反映 blur readiness 限制。

Quickshell 自己也會動畫 shell surface 與 wallpaper transition。GPU 效能與 application 相容性仍需真實 session 驗證。詳見 [Graphics](GRAPHICS.md) 與 [Architecture](ARCHITECTURE.md)。

## NyxNiri 桌面整合、Orbit 與動態桌布

新增模組、色盤與 portal 服務、快捷鍵、相依套件及驗證界線，請參閱[桌面整合說明](NYXNIRI_DESKTOP.md)。
