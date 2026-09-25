# NyxNiri-inspired desktop — 1.0.1a

[Traditional Chinese](../zh/NYXNIRI_DESKTOP.md) · [Documentation index](README.md)

This integration follows [ech678/NyxNiri](https://github.com/ech678/NyxNiri) at `adf0954c877d2c70de12a51b9fd6727a59352f98`. The reference combines Niri, Noctalia and application configuration. LunaDash keeps its C++20/wlroots compositor, C11 rendering helpers, Quickshell shell, module registry and plugin interfaces. It does not start Niri, Hyprland or a second desktop shell. Existing user preferences and shortcut assignments remain authoritative.

## Desktop and controls

New profiles follow the supplied reference screenshots: an inset top panel with numbered workspaces, running applications, the active window title and CPU/memory readings on the left; a centered launcher; and status controls and the clock on the right. The workspace list shows occupied workspaces, the active workspace and one spare empty workspace. This changes only the panel display; all configured workspaces and their shortcuts remain available. The compact control center opens on the right with controls, media, audio and system pages. Settings > Dashboard can select the larger dashboard. Settings keeps its shared header and search.

The bottom shortcut dock and large wallpaper clock are disabled by default, leaving the wallpaper clear as in the reference. Settings > Appearance > Show dock can enable the dock; the Desktop widgets extension settings can enable the wallpaper clock. Existing saved preferences are preserved. If enabled, dock pins come from the `dock` module configuration and running tasks use the existing activation behavior. Auto-hide leaves an 8 px reveal strip while the current workspace has visible applications.

The default window arrangement keeps the first window in the right half and divides the focused tile for later windows, alternating split axes. This creates the reference's large right pane and recursively divided left panes through the existing bounded tiling template. Settings > Windows and workspaces lets users change the split target and initial side; see [window layout templates](WINDOW_LAYOUT_TEMPLATES.md).

The `orbit` and `dock` modules use the existing module schema and extension slots. `desktop-widgets` remains a multiple-selection extension target inside the wallpaper Background layer. Its built-in settings control the clock position and optional playback audio rings. Rings use the existing output-monitor spectrum helper, not microphone input. Weather is off initially: enabling it sends the entered coordinates, rounded to three decimal places, to [Open-Meteo](https://open-meteo.com/en/docs) at startup, on location changes and every 15 minutes. Failed requests show unavailable data. No location lookup runs automatically.

| Default shortcut | Action |
| --- | --- |
| Super+A / Super+mouse forward | Orbit |
| Super+W | Wallpaper picker |
| Super+Ctrl+W | Random wallpaper |
| Super+N | Eye care |
| Super+grave | Persistent terminal scratchpad |
| Super+I | Control center |
| Super+V | Clipboard history |
| Super+X | Session controls |
| Super+Shift+T | Floating / tiled window |
| Super+Shift+F | Fullscreen |

Super+T and Super+Return retain the configured terminal role. New defaults yield to existing custom shortcut assignments. The scratchpad tracks the launched terminal process and its descendants, hides without closing, and follows the current workspace when shown. A terminal configured to forward every launch into an existing server may need an argument that creates a separate process. Launch failures and the 15-second adoption timeout appear in `scratchpad.error` in the control status. Floating windows support Alt+drag and Alt+Shift+drag; client-initiated move/resize still requires a valid input serial.

## Editable desktop configuration

Settings > Appearance > Panel layout controls the centered launcher, active window title, CPU/memory readings and compact workspace list. Turning off the centered launcher restores the launcher on the left and the clock in the center. Settings > Shell modules exposes the remaining validated panel fields, including dimensions, edge, colors, capsule contrast and workspace indicators. These controls use the existing module registry and save through the existing settings backend.

The corresponding file is `$XDG_CONFIG_HOME/LuDash/shell-modules.json`, normally `~/.config/LuDash/shell-modules.json`. Edit the existing document's `modules.panel.config` object to customize it:

```json
{
  "centerLauncher": true,
  "showSystemStats": true,
  "showActiveTitle": true,
  "occupiedWorkspacesOnly": true
}
```

This is a fragment, not a replacement for the whole file. Omitted fields receive registry defaults when a document is saved, so preserve the surrounding module settings. External edits are watched and invalid values leave the last valid configuration active. `data/modules/templates/shell-modules.json` provides a minimal example of the default top-panel layout. Other appearance controls, including dock visibility, remain desktop preferences; application roles, shortcuts, Orbit and plugins retain their existing configuration owners. This offers an editable author-style setup using LunaDash's supported JSON and settings interfaces. It does not parse Niri KDL or import another compositor's runtime.

For the first-install wizard, optional applications, tools and author application-profile import, see [session installation](LOGIN_SESSION.md). The installer preserves existing user files; imported application settings remain editable in their owning applications.

## Orbit configuration

Settings > Appearance includes the JSON editor, also stored at `$XDG_CONFIG_HOME/lunadash/orbit.json` (default `~/.config/lunadash/orbit.json`). The shipped template is `data/launcher/orbit.json`. It defines apps, nested folders, web links and configurable web/AI search destinations. Tab/Shift+Tab changes engines, Alt+1–8 activates an item, and Escape returns to the parent or closes Orbit. Search launches the configured URL with an encoded query; this is a browser shortcut, not a built-in AI client.

Documents require `schemaVersion: 1`, `defaultEngine`, `searchEngines` and `items`. There are 1–8 engines, 1–8 entries per folder and at most three folder levels. Every entry has a unique sibling `id`, a `name` and exactly one of `action`, `desktopId`, `command`, `url` or `children`. Commands are argument arrays; selected files and search text are never evaluated as shell code. Only HTTP(S) URLs are accepted. Invalid or oversized edits leave the saved configuration intact. The editor can restore the bundled template.

## Wallpapers and appearance

The wallpaper library searches the configured absolute directory and one category-directory level, capped at 512 files. It combines that inventory with the bundled wallpaper and recent choices. Search, static/live filters, category selection, random selection and manual local paths share the existing picker. Static images are bounded to 64 MiB and 32 megapixels; videos to 2 GiB. Supported video extensions are MP4, WebM, MKV, MOV and M4V, subject to installed codecs. Playback is muted and loops behind all windows. FFmpeg generates cached poster frames; missing multimedia support reports an error and retains the available poster. Video previews are generated for the selected video, not every file in the library.

Wallpaper colors derive a tonal palette from a small image/poster sample. This is LunaDash's palette algorithm, not Noctalia's exact Material HCT implementation. Light, dark and automatic modes share it across QML and native desktop dialogs. Automatic mode uses light colors from 07:00 to 19:00 local time. Disabling wallpaper colors allows manual accent settings.

Eye care makes shell surfaces opaque and applies a configurable 2500–6500 K output transform. wlroots 0.20 uses a scene color transform; older supported versions use output gamma ramps when the backend supports them. Unsupported outputs report an error rather than claiming the transform succeeded. Ordinary Wayland and XWayland windows now share the compositor backdrop blur path; see [Visual effects](EFFECTS.md) for opacity controls and verification limits. Appearance presets save validated visual settings under `lunadash/presets`; they do not capture application data, external files or the video itself.

The Settings portal publishes color scheme, contrast and reduced-motion hints. Optional application-theme synchronization writes LunaDash-owned GTK 3/4 CSS and Kitty color files, adds includes, sets GTK's dark preference and requests the GNOME color-scheme hint where available. Kitty picks up generated colors on reload/restart; browser support depends on its portal integration. Optional Fcitx synchronization renders the bundled Mellow templates and selects that theme through classicui. Each modified pre-existing CSS, Kitty or Fcitx configuration receives a first-use `.lunadash-backup`. Turning synchronization off stops further updates; it does not restore those external files automatically. To undo integration, remove the LunaDash include or restore the backup, and select your previous GTK/Fcitx theme. Keep personal additions in separate includes.

The Mellow templates derive from NyxMellow and retain the upstream GPL-3.0 license and attribution in `data/themes/nyxmellow/`. Other UI code uses LunaDash's shared components; no upstream wallpaper pack is redistributed.

## Dependencies and verification boundaries

Arch runtime packaging includes `qt6-multimedia` and `ffmpeg` for live wallpapers, alongside the existing Quickshell, Qt Wayland, wlroots, Fcitx and PulseAudio/PipeWire tools. Qt Multimedia is loaded only when a video is selected, so the native build does not require its development headers. On other distributions install the Qt 6 Multimedia **QML module**, a functioning multimedia backend/codecs, and FFmpeg using that distribution's packages (Debian/Ubuntu commonly `qml6-module-qtmultimedia`; Fedora `qt6-qtmultimedia`). Static wallpaper operation does not require video codecs. Weather uses Qt Network, already a build dependency. Portable CMake and compatibility branches are not evidence of verification on every distribution or GPU.

This is a development integration, not complete NyxNiri parity. Niri-specific tabbed columns, cross-output navigation and glow presets; Noctalia's greeter/secure lock protocols; the upstream whole-application preset/deployment/rollback system; and downloadable wallpaper packs remain outside this implementation. Optional author application-profile import is handled by LunaDash's installer, independently of its compositor. LunaDash's existing tiling/stacking templates, updater, session controls and external application roles remain in use. Native plugins stay disabled by default and are not sandboxed.

For this layout and installation change, the user requested no local tests or builds, including local source/QML/translation audits. Verification is deferred to the configured CI workflows after pushing; workflow configuration alone does not establish a passing result. No real-session screenshot has been produced for this change, so final visual verification remains outstanding.
