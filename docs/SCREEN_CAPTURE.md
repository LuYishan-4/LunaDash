# Screen capture

Press **Meta+Shift+S** (Super+Shift+S) to select a rectangular region. Drag to select; press Escape to cancel. LunaDash starts `slurp` in its own Wayland session, waits for the selection overlay to close, and passes that geometry to `grim`. Both helpers are included in the installer dependencies. The compositor continues processing input and frames while you select.

The screenshot is saved as a private PNG in Pictures/Screenshots. Cancelling leaves the previous screenshot untouched and writes no image. The previous shipped Alt+Shift+F5 binding is migrated unless Meta+Shift+S is already assigned to another action; custom bindings and disabled actions are preserved.

The selector receives an empty standard input so it opens immediately instead of waiting for rectangle candidates. Pressing the shortcut again while selecting keeps the current selection and keyboard focus; Escape still cancels without an error notification.

`lunadashctl screenshot` starts the same interactive selection and immediately returns `pending`/`phase`, rather than blocking until a path is available. Poll `lunadashctl status` for `screenCapture.phase` (`selecting`, `capturing`, `saved`, `cancelled`, `failed`), `busy`, `lastCapture` and `error`. The shell reports saved files and failures through notifications. Missing helpers produce an error rather than silently capturing the entire display.

For automation, `lunadashctl capture /absolute/path.png` remains an explicit full-output capture and refuses to overwrite an existing file. `lunadash-compositor --screenshot PATH` also retains full-output capture for test runs. These commands do not open a region selector.

The compositor advertises wlroots screencopy, xdg-output and idle-inhibit globals. The capture unit test verifies helper startup, end-of-input handling, cancellation diagnostics and recovery without opening a display. The headless desktop-controls regression uses a fixture selector with real `grim` and verifies PNG dimensions, errors, permissions and responsive IPC. The screenshot-shortcut regression sends actual keyboard and pointer events through an isolated Xvfb host to real `slurp` and `grim`: it checks the selection overlay, dragged region dimensions, repeated shortcuts, Escape and reopening. CI retains `screenshot-shortcut.log` and `shortcut-region.png`. These tests do not verify physical input devices or every hardware buffer format. Test HiDPI/multiple-output geometry in a real session before release promotion.

The file chooser portal is separate from capture. A complete PipeWire screen-sharing portal, per-toplevel capture policy and screen-locking integration remain incomplete. See [graphics](GRAPHICS.md) and [testing](TESTING_AND_FILES.md).
