# Screen capture

The compositor creates wlroots screencopy, xdg-output and idle-inhibit globals. Capture behavior and supported buffers follow the installed wlroots renderer/output implementation, rather than the previous Qt compositor's fixed protocol versions.

`lunadashctl screenshot` (also the default Alt+Shift+F5 action) chooses an unused PNG path in the user's Pictures/Screenshots directory. `lunadashctl capture /absolute/path.png` requests an explicit path and refuses to overwrite an existing file. The compositor launches `grim` in the isolated session environment; install grim to use this action. Status reports the most recent path or error under `screenCapture`.

The current CI checks screencopy and related protocol globals. Inspect actual captured content in a real session; a global being advertised does not establish every dmabuf format, cursor behavior or hardware encoder path. The historical screenshot test targets the previous Qt compositor and is not current wlroots release evidence.

The file chooser portal is separate from capture. LunaDash does not yet implement a complete PipeWire screen-sharing portal, per-toplevel capture policy or screen-locking integration. See [graphics](GRAPHICS.md) and [testing](TESTING_AND_FILES.md).
