# Media panel

The overview's Media tab has a circular album cover and playback animation, a central lyrics area, a small animated cat, and shared LunaDash controls. The ring is a decorative playback indicator, not an audio spectrum measurement. Animation stops while paused, hidden, or disabled in Appearance.

The player selector can follow the currently playing source or pin a particular player. Previous/next, play/pause, seeking, player volume, shuffle, repeat and showing the player window use the capabilities reported by that player. Unsupported controls are disabled. Repeat cycles through off, playlist and track. Player volume is separate from the desktop's system volume.

`lunadash-shell-tool media-status` discovers registered MPRIS players on both the session bus and the login user's runtime bus. This covers sessions whose private bus differs from the bus used by systemd user services or sandboxed applications. Identical buses are queried once. Properties are fetched concurrently with bounded timeouts; one unavailable player does not prevent others from appearing. Commands address the selected service **and** bus, and never switch an action to another player if the selection closes. Seek requests include the track ID to avoid applying an old seek to a new song.

Lyrics come from player-supplied text (`xesam:lyrics`, `lyrics` or `xesam:asText`) or a same-name `.lrc` file beside a local track. Timestamped lyrics follow playback; plain lyrics can be scrolled. A player that supplies neither shows a clear empty state. LunaDash does not fetch lyrics from an external provider. Browsers and applications must expose MPRIS playback for this panel to control them; it does not capture arbitrary application audio or window pixels.

The implementation follows the [MPRIS player interface](https://specifications.freedesktop.org/mpris/latest/Player_Interface.html). The native media helper owns discovery and D-Bus actions under `src/shell/media/`; QML presentation and lyrics parsing live under `qml/overview/`. Tests create isolated D-Bus services and do not control the user's real players.
