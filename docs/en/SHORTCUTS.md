# Keyboard shortcuts

[繁體中文](../zh/SHORTCUTS.md)

These are the shipped desktop defaults. **Meta / Super** is usually the Windows key. Settings > Keyboard shortcuts shows the current bindings, including custom values and disabled actions; the tables below do not replace saved choices. There are 51 configurable actions (31 commands and 20 workspace bindings). The source is `src/desktop/shortcuts/ShortcutSettings.cpp`.

| Keys | Action |
| --- | --- |
| Meta+H / L / K / J | Focus left / right / up / down |
| Meta+Shift+H / L | Group with the left / right column |
| Meta+Ctrl+H / L | Move the column left / right |
| Meta+Shift+E | Expel the focused window from its group |
| Meta+Shift+C | Center the column |
| Meta+= / Meta+- | Widen / narrow the column |
| Meta+F | Maximize / restore |
| Meta+Shift+F | Enter / leave fullscreen |
| Meta+Shift+T | Toggle floating / tiled |
| Meta+C or Meta+Q | Close the focused window |
| Meta+M | Minimize the focused window |
| Meta+T or Meta+Enter | Open the configured terminal |
| Meta+E | Open the configured file manager (Dolphin by default) |
| Meta+D | Open application search |
| Meta+A | Open Orbit launcher |
| Meta+Shift+S | Take a screenshot |
| Meta+W | Open the wallpaper library |
| Meta+Ctrl+W | Choose a random wallpaper |
| Meta+N | Toggle eye care |
| Meta+` (grave accent) | Toggle terminal scratchpad |
| Meta+I | Open control center |
| Meta+V | Open clipboard history |
| Meta+X | Open session/power controls |
| Meta+1…9, Meta+0 | Switch to workspace 1…9, 10 |
| Meta+Shift+1…9, Meta+Shift+0 | Move the focused window to workspace 1…9, 10 |

Workspace switching requires the destination workspace to be enabled in Settings. Layout operations follow the active template; grouping/column actions can be unavailable in a replacement template.

The following controls are fixed and also listed at the bottom of Settings > Keyboard shortcuts:

| Keys / gesture | Action |
| --- | --- |
| Tap Meta alone | Open application search |
| F12 | Toggle fullscreen |
| Alt+Tab / Alt+Shift+Tab | Step forward / backward in the window switcher |
| Arrow keys in switcher | Select; left/right step one item, up/down step five |
| Enter or release Alt in switcher | Confirm selection |
| Escape in switcher | Cancel |
| Alt+left-button drag | Move floating windows or rearrange tiled windows |
| Alt+Shift+left-button drag | Resize the window/layout |

Within shell surfaces, application search uses Up/Down to select, Enter to launch and Escape to close. In Orbit's search field, Tab/Shift+Tab cycles search engines and Alt+1…8 activates an item; focused items accept Enter/Space, and Escape goes back. Settings uses Ctrl+F to focus its header search; Escape clears that search, then closes Settings when it is empty. The shortcut recorder uses Enter/Space to begin, Escape to cancel and Backspace to disable while recording. Focused workspace buttons accept Enter/Space. Escape closes the desktop context menu and image picker. Ordinary Tab/Shift+Tab navigation and the shortcuts of external applications remain owned by their respective controls/apps.

The built-in `welcome` tool keeps its stable command ID, but its visible window title and startup selector use the translated **Welcome** label (**歡迎** with Traditional Chinese selected).
