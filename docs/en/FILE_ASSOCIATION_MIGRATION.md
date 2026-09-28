# File associations after switching to Dolphin

The built-in LunaDash file manager and its private association store are retired. Dolphin and other configured external managers use their own preferences and system MIME handlers.

LunaDash no longer reads or migrates `$XDG_CONFIG_HOME/LunaDash/file-associations.json` or the earlier QSettings `fileAssociations/*` keys. Existing files are left intact for reference; nothing is automatically imported into Dolphin or deleted. Re-select document handlers through Dolphin or the system default-application settings when needed. System MIME defaults previously changed by the user remain effective.

The files application role remains configurable in Settings > Applications and startup. An empty command selects Dolphin. FileChooser and screen/window sharing pickers remain part of LunaDash. See [Files](FILES.md).
