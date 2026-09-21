# Media 面板

[English](../en/MEDIA.md) · [繁中索引](README.md)

Overview 的 Media tab 有圓形 album cover、播放動畫、中央 lyrics、Togawa Sakiko mascot 與 LunaDash 共用控制。外圈只是 playback indicator，不是音訊 spectrum。Pause、隱藏或停用動畫時會停止。

Mascot GIF 位於 `qml/overview/assets/togawa-sakiko-ave-mujica.gif`，本機載入、保持 aspect ratio，不需要每次開 panel 都抓外部資源。

## MPRIS

Player selector 可以跟隨目前 playing source，也能 pin 特定 player。若 player 宣告支援，介面可提供：

- previous / next
- play / pause
- seek
- player volume
- shuffle
- repeat（off / playlist / track）
- show player window

Player volume 和 desktop system volume 是不同概念。

`lunadash-shell-tool media-status` 會同時查 session bus 與登入使用者 runtime bus 的 MPRIS service，以涵蓋 private D-Bus session 與 systemd-user/sandbox app 分開的情況。重複 bus 只查一次；property request 並行且有 bounded timeout；單一壞掉 player 不會拖住其他 player。

Command 同時綁定 selected **service + bus**，若 player 關閉不會把 action 偷換到另一個 player。Seek 會帶 track ID，避免 song 已換掉時把舊 seek 套上去。

## Lyrics

來源依序可以是 player metadata 的 `xesam:lyrics`、`lyrics`、`xesam:asText`，或 local track 旁同名 `.lrc`。Timestamped lyrics 會跟 playback；plain text 可 scroll。LunaDash 不向外部 lyric provider 抓歌詞。

Browser/app 必須自己提供 MPRIS 才能被控制；Media panel 不擷取 arbitrary application audio/window pixels。

實作：native helper 在 `src/shell/media/`，QML 與 lyrics parser 在 `qml/overview/`。測試使用隔離 D-Bus service，不會控制使用者真實 player。
