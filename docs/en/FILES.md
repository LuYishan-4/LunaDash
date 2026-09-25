# Files and Dolphin

LunaDash uses **Dolphin** as its default file manager. The installer and Arch package include `dolphin` as a core runtime dependency. The optional desktop bundle adds Chromium, Ark and KIO extras. Builds on other distributions do not by themselves verify Dolphin integration there.

Super+E, Files shortcuts and `lunadashctl launch-default files` use the files role. An empty role command selects `dolphin --new-window`, opening a new window on the current workspace. Settings > Applications and startup can select another installed application; existing explicit choices remain respected. `lunadash-desktop --app files --path /absolute/folder` forwards the folder as a single argument, without a shell.

The former native file manager, its private association UI, GIO launcher and copy/move/trash engine are removed from source and build targets. There is no built-in fallback. Dolphin owns browsing, file operations and its application preferences. Configure file-type handlers through Dolphin or the system MIME settings. Old LunaDash association files are left untouched but are no longer read or imported; see [migration](FILE_ASSOCIATION_MIGRATION.md).

## Retained pickers

The LunaDash portal FileChooser keeps its frameless dark header, manual local-path field, filters, multiple selection, save confirmation and bounded previews. Its icon implementation lives under `src/service/portal/`; it does not depend on a file-manager application. The screen/window sharing chooser and Settings image picker remain available. Paths are validated without shell evaluation. See [default applications and pickers](DEFAULT_APPS_AND_FILES.md).

The portal follows the LunaDash palette. Dolphin controls its own application theme; compositor opacity and frosted-glass settings apply to its windows like other ordinary applications. Fullscreen and eye-care exceptions remain unchanged.

## CI coverage

Native Qt tests cover role defaults, explicit overrides and retained portal selection behavior. The Arch Quickshell workflow launches actual Dolphin windows and checks workspace switching, usable area, recursive tiling and rounded/glass pixels. It uploads software Wayland session screenshots. Physical GPU and login-session appearance still need separate verification.
