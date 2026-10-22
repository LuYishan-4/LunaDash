export interface Method {
  category: string;
  name: string;
  value: string;
  behavior: string;
  example: string;
}

export interface ApiField {
  name: string;
  type: string;
  purpose: string;
}

export const methods: Method[] = [
  {
    "category": "State and settings",
    "name": "status",
    "value": "Empty",
    "behavior": "Return the complete compositor snapshot: windows, workspaces, appearance, displays, input, audio, network, power, plugins, modules, capture state and Settings API descriptors.",
    "example": "lunadashctl status"
  },
  {
    "category": "State and settings",
    "name": "settings-describe",
    "value": "Empty",
    "behavior": "Return Settings API v1 and every editable target. Each descriptor contains id, schema, values and a SHA-256 revision token.",
    "example": "lunadashctl settings-describe"
  },
  {
    "category": "State and settings",
    "name": "settings-update",
    "value": "JSON {\"target\":\"...\",\"revision\":\"...\",\"changes\":{...}}",
    "behavior": "Patch one Settings API target. The revision must still match; unknown fields, stale revisions, read-only changes and schema-invalid values are rejected. The request is capped at 24 KiB.",
    "example": "lunadashctl settings-update '{\"target\":\"builtin:window-animation\",\"revision\":\"<revision>\",\"changes\":{\"duration\":260}}'"
  },
  {
    "category": "State and settings",
    "name": "appearance",
    "value": "Partial desktop-preferences JSON",
    "behavior": "Validate and atomically apply a desktop preference patch; invalid or unknown fields reject the update.",
    "example": "lunadashctl appearance '{\"gap\":16,\"animations\":true}'"
  },
  {
    "category": "State and settings",
    "name": "reset-preferences",
    "value": "Empty",
    "behavior": "Remove the desktop preference group, then reapply keyboard configuration and layout. Plugin/module/default-app documents are separate.",
    "example": "lunadashctl reset-preferences"
  },
  {
    "category": "State and settings",
    "name": "open-settings",
    "value": "Empty or a built-in page id",
    "behavior": "Open Settings at general, appearance, windows, shortcuts, display, input, input-method, sound, network, bluetooth, power, applications, privacy, system, devices, about, modules, plugins or dashboard.",
    "example": "lunadashctl open-settings plugins"
  },
  {
    "category": "Windows and workspaces",
    "name": "workspace",
    "value": "Zero-based workspace index",
    "behavior": "Switch to an existing workspace. The index must be within appearance.workspaceCount.",
    "example": "lunadashctl workspace 1"
  },
  {
    "category": "Windows and workspaces",
    "name": "focus",
    "value": "Positive client id",
    "behavior": "Switch to the client's workspace, restore it if minimized, focus it in the layout and give the surface keyboard focus.",
    "example": "lunadashctl focus 12"
  },
  {
    "category": "Windows and workspaces",
    "name": "activate-window",
    "value": "Positive client id",
    "behavior": "Activate a task/window through the compositor task activation path.",
    "example": "lunadashctl activate-window 12"
  },
  {
    "category": "Windows and workspaces",
    "name": "minimize",
    "value": "Positive client id",
    "behavior": "Minimize the client, update XWayland state when applicable, update the layout and synchronize focus.",
    "example": "lunadashctl minimize 12"
  },
  {
    "category": "Windows and workspaces",
    "name": "close",
    "value": "Positive client id",
    "behavior": "Request a normal application close; this is not a process kill.",
    "example": "lunadashctl close 12"
  },
  {
    "category": "Windows and workspaces",
    "name": "group-window",
    "value": "JSON {\"window\":ID,\"target\":ID}",
    "behavior": "Ask the active layout to group one window with another. The layout must support group-with.",
    "example": "lunadashctl group-window '{\"window\":12,\"target\":7}'"
  },
  {
    "category": "Windows and workspaces",
    "name": "expel-window",
    "value": "Positive client id",
    "behavior": "Ask the active layout to expel a window from its current group.",
    "example": "lunadashctl expel-window 12"
  },
  {
    "category": "Windows and workspaces",
    "name": "window-layout-settings",
    "value": "JSON matching status.windowLayout.schema",
    "behavior": "Patch the active layout template settings and rearrange immediately.",
    "example": "lunadashctl window-layout-settings '{\"gap\":12}'"
  },
  {
    "category": "Windows and workspaces",
    "name": "window-layout-action",
    "value": "JSON {\"action\":\"<id>\",\"payload\":{...}}",
    "behavior": "Invoke an action advertised by status.windowLayout.actions. Only action and payload are accepted; required work-area data is injected by the compositor.",
    "example": "lunadashctl window-layout-action '{\"action\":\"expel\",\"payload\":{\"window\":12}}'"
  },
  {
    "category": "Windows and workspaces",
    "name": "switch-window",
    "value": "Positive client id",
    "behavior": "Select a particular item in the active compositor window switcher.",
    "example": "lunadashctl switch-window 12"
  },
  {
    "category": "Windows and workspaces",
    "name": "switch-step",
    "value": "previous or next",
    "behavior": "Step the switcher backward for previous, otherwise forward.",
    "example": "lunadashctl switch-step next"
  },
  {
    "category": "Windows and workspaces",
    "name": "switch-accept",
    "value": "Empty",
    "behavior": "Accept the current window-switch selection.",
    "example": "lunadashctl switch-accept"
  },
  {
    "category": "Windows and workspaces",
    "name": "switch-cancel",
    "value": "Empty",
    "behavior": "Cancel the current window-switch operation.",
    "example": "lunadashctl switch-cancel"
  },
  {
    "category": "Windows and workspaces",
    "name": "launcher-visible",
    "value": "true or false",
    "behavior": "Synchronize launcher visibility with the compositor; primarily used by the LunaDash shell.",
    "example": "lunadashctl launcher-visible true"
  },
  {
    "category": "Input and shortcuts",
    "name": "shortcut-capture",
    "value": "true or false",
    "behavior": "Enable/disable shortcut-capture mode while Settings records a key combination.",
    "example": "lunadashctl shortcut-capture true"
  },
  {
    "category": "Input and shortcuts",
    "name": "shortcuts",
    "value": "Partial JSON mapping known action ids to shortcut strings",
    "behavior": "Update bindings. Values are Disabled or a Meta/Alt key combination; duplicates and Alt+Tab / Alt+Shift+Tab are rejected.",
    "example": "lunadashctl shortcuts '{\"launchTerminal\":\"Meta+Return\",\"closeWindow\":\"Meta+Q\"}'"
  },
  {
    "category": "Input and shortcuts",
    "name": "reset-shortcuts",
    "value": "Empty",
    "behavior": "Restore the built-in shortcut map, including workspace1..10 and moveToWorkspace1..10.",
    "example": "lunadashctl reset-shortcuts"
  },
  {
    "category": "Input and shortcuts",
    "name": "send-key",
    "value": "copy, paste, cut or selectAll",
    "behavior": "Inject the corresponding Ctrl chord into the currently focused wlroots keyboard surface.",
    "example": "lunadashctl send-key copy"
  },
  {
    "category": "Applications and URLs",
    "name": "default-apps",
    "value": "JSON object of command arrays",
    "behavior": "Save default terminal/files/browser command arrays. Empty terminal/files commands can use LunaDash fallbacks.",
    "example": "lunadashctl default-apps '{\"browser\":[\"google-chrome-stable\"]}'"
  },
  {
    "category": "Applications and URLs",
    "name": "launch-default",
    "value": "terminal, files or browser",
    "behavior": "Launch the configured default. Terminal/files can fall back to LunaDash built-ins; browser requires an available browser command.",
    "example": "lunadashctl launch-default browser"
  },
  {
    "category": "Applications and URLs",
    "name": "launch-command",
    "value": "Non-empty JSON command array",
    "behavior": "Launch a program directly without a shell. Every argument is a string up to 1024 characters.",
    "example": "lunadashctl launch-command '[\"foot\",\"-e\",\"htop\"]'"
  },
  {
    "category": "Applications and URLs",
    "name": "launch-with-x11",
    "value": "JSON command array; CLI also accepts -- program args",
    "behavior": "Launch through the external-command path with authenticated X11 compatibility prepared.",
    "example": "lunadashctl launch-with-x11 -- xterm -e htop"
  },
  {
    "category": "Applications and URLs",
    "name": "launch-x11",
    "value": "Command-line string parsed by QProcess::splitCommand",
    "behavior": "Launch directly through XWayland. This parses arguments; it does not evaluate shell operators.",
    "example": "lunadashctl launch-x11 'xterm -e htop'"
  },
  {
    "category": "Applications and URLs",
    "name": "launch-application",
    "value": "JSON exactly {\"desktopId\":\"...\",\"command\":[...]}",
    "behavior": "Launch a desktop-app command after capability resolution. desktopId <=256 chars; command has 1–64 non-empty strings, each <=1024 chars.",
    "example": "lunadashctl launch-application '{\"desktopId\":\"org.example.App\",\"command\":[\"example-app\"]}'"
  },
  {
    "category": "Applications and URLs",
    "name": "open-url",
    "value": "URL string",
    "behavior": "Resolve the configured browser command and open the URL.",
    "example": "lunadashctl open-url https://github.com/LuYishan-4/LunaDash"
  },
  {
    "category": "Applications and URLs",
    "name": "system-tool",
    "value": "Id from status.settingsTools",
    "behavior": "Open one validated system-settings helper; discover ids through status rather than guessing executables.",
    "example": "lunadashctl system-tool '<tool-id>'"
  },
  {
    "category": "Plugins",
    "name": "extension-save",
    "value": "Complete extensions schemaVersion 1 JSON",
    "behavior": "Validate and atomically save built-in/plugin configuration, refresh PluginManager and rearrange. Maximum size: 24 KiB.",
    "example": "lunadashctl extension-save '{\"schemaVersion\":1,\"builtins\":{},\"plugins\":{}}'"
  },
  {
    "category": "Plugins",
    "name": "extension-install",
    "value": "Plugin id from status.extensions.remote",
    "behavior": "Start an install from the reviewed Store catalogue. The immediate response is pending; progress/error lives in extensions.remote.",
    "example": "lunadashctl extension-install org.example.plugin"
  },
  {
    "category": "Plugins",
    "name": "extension-remove",
    "value": "User-installed plugin id",
    "behavior": "Disable and remove only a plugin under the canonical user LunaDash plugin root. System plugins cannot be removed here.",
    "example": "lunadashctl extension-remove org.example.plugin"
  },
  {
    "category": "Plugins",
    "name": "extension-error",
    "value": "JSON {\"id\":\"package@target\",\"error\":\"message\"}",
    "behavior": "Report a plugin runtime failure; messages are capped at 1024 chars. Send an empty error to clear quarantine and retry/refresh.",
    "example": "lunadashctl extension-error '{\"id\":\"org.example.panel@panel\",\"error\":\"\"}'"
  },
  {
    "category": "Shell modules",
    "name": "module-validate",
    "value": "Complete shell-modules JSON",
    "behavior": "Validate the versioned module document without writing it.",
    "example": "lunadashctl module-validate \"$(cat shell-modules.json)\""
  },
  {
    "category": "Shell modules",
    "name": "module-save",
    "value": "Complete shell-modules JSON",
    "behavior": "Validate, atomically save and apply the module document.",
    "example": "lunadashctl module-save '<json>'"
  },
  {
    "category": "Shell modules",
    "name": "module-template",
    "value": "Template id",
    "behavior": "Install a supported starter module file without overwriting an existing user file.",
    "example": "lunadashctl module-template panel"
  },
  {
    "category": "Shell modules",
    "name": "module-code-trust",
    "value": "true or false",
    "behavior": "Opt in/out of executing user custom QML; this code is trusted user code, not sandboxed.",
    "example": "lunadashctl module-code-trust true"
  },
  {
    "category": "Shell modules",
    "name": "module-reset",
    "value": "Empty",
    "behavior": "Restore built-in module settings and disable custom-code trust while preserving user QML files.",
    "example": "lunadashctl module-reset"
  },
  {
    "category": "Shell modules",
    "name": "module-error",
    "value": "JSON {\"id\":\"module-id\",\"error\":\"message\"}",
    "behavior": "Report or clear a custom-module runtime error so recovery content remains available.",
    "example": "lunadashctl module-error '{\"id\":\"panel\",\"error\":\"load failed\"}'"
  },
  {
    "category": "Audio, network and power",
    "name": "audio",
    "value": "JSON with exactly device plus id, volume or mute",
    "behavior": "device is output/input. Select output with id, set integer volume 0..100, or set mute boolean. Requires wpctl/WirePlumber and no pending audio change.",
    "example": "lunadashctl audio '{\"device\":\"output\",\"volume\":50}'"
  },
  {
    "category": "Audio, network and power",
    "name": "network",
    "value": "JSON with \"action\" and action-specific fields",
    "behavior": "Structured NetworkManager/Bluetooth operations; accepted actions are documented below. Read status.network for current names/devices first.",
    "example": "lunadashctl network '{\"action\":\"wifi-connect\",\"ssid\":\"My WiFi\",\"password\":\"secret\"}'"
  },
  {
    "category": "Audio, network and power",
    "name": "power-profile",
    "value": "Profile from status.power.profiles",
    "behavior": "Switch with powerprofilesctl to an advertised profile such as power-saver, balanced or performance.",
    "example": "lunadashctl power-profile balanced"
  },
  {
    "category": "Audio, network and power",
    "name": "session-action",
    "value": "suspend, reboot or poweroff",
    "behavior": "Call systemd-logind only when status.sessionActions reports the action available.",
    "example": "lunadashctl session-action suspend"
  },
  {
    "category": "Displays",
    "name": "brightness",
    "value": "Integer 1..100",
    "behavior": "Set the active backlight through brightnessctl; status.brightness must report an available device.",
    "example": "lunadashctl brightness 75"
  },
  {
    "category": "Displays",
    "name": "ddc-brightness",
    "value": "JSON {\"id\":\"<device-id>\",\"percent\":0..100}",
    "behavior": "Set and verify external-monitor VCP 0x10 brightness through ddcutil.",
    "example": "lunadashctl ddc-brightness '{\"id\":\"i2c-6:DELL U2720Q\",\"percent\":60}'"
  },
  {
    "category": "Displays",
    "name": "ddc-refresh",
    "value": "Empty",
    "behavior": "Rescan DDC/CI displays and query current brightness.",
    "example": "lunadashctl ddc-refresh"
  },
  {
    "category": "Displays",
    "name": "display-configure",
    "value": "JSON containing mode and/or scale",
    "behavior": "Test+commit a primary-display change. mode must be advertised; scale is 1.0..3.0. Success starts a 15-second confirmation timer.",
    "example": "lunadashctl display-configure '{\"scale\":1.25}'"
  },
  {
    "category": "Displays",
    "name": "display-confirm",
    "value": "Empty",
    "behavior": "Persist the pending display width/height/refresh/scale before the 15-second revert timer expires.",
    "example": "lunadashctl display-confirm"
  },
  {
    "category": "Displays",
    "name": "display-revert",
    "value": "Empty",
    "behavior": "Immediately restore the display state saved before the pending change.",
    "example": "lunadashctl display-revert"
  },
  {
    "category": "Displays",
    "name": "desktop-size",
    "value": "Nested output size",
    "behavior": "Resize a supported windowed nested session; physical/fullscreen sessions reject unsupported resizing.",
    "example": "lunadashctl desktop-size 1440x900"
  },
  {
    "category": "Capture and wallpaper",
    "name": "capture",
    "value": "Absolute new output path",
    "behavior": "Start an asynchronous screenshot save. Relative paths and existing files are rejected; response includes path/pending/phase.",
    "example": "lunadashctl capture /tmp/lunadash-shot.png"
  },
  {
    "category": "Capture and wallpaper",
    "name": "screenshot",
    "value": "Empty",
    "behavior": "Start the normal LunaDash screenshot flow; read status.screenCapture for completion/error.",
    "example": "lunadashctl screenshot"
  },
  {
    "category": "Capture and wallpaper",
    "name": "wallpaper",
    "value": "0 or 1",
    "behavior": "Select a built-in shader wallpaper palette and set wallpaperMode to shader.",
    "example": "lunadashctl wallpaper 1"
  },
  {
    "category": "Capture and wallpaper",
    "name": "choose-wallpaper",
    "value": "Empty",
    "behavior": "Open Settings → Appearance and signal the image picker.",
    "example": "lunadashctl choose-wallpaper"
  },
  {
    "category": "Capture and wallpaper",
    "name": "wallpaper-image",
    "value": "Absolute local path or file URL",
    "behavior": "Validate and select an image wallpaper.",
    "example": "lunadashctl wallpaper-image /home/user/Pictures/wallpaper.png"
  },
  {
    "category": "Capture and wallpaper",
    "name": "wallpaper-default",
    "value": "Empty",
    "behavior": "Clear the custom image wallpaper and restore the default image state.",
    "example": "lunadashctl wallpaper-default"
  },
  {
    "category": "Language, setup and maintenance",
    "name": "language",
    "value": "Supported locale id",
    "behavior": "Save a supported LunaDash UI locale.",
    "example": "lunadashctl language zh_TW"
  },
  {
    "category": "Language, setup and maintenance",
    "name": "configure-network",
    "value": "Empty",
    "behavior": "Open nm-connection-editor, otherwise run nmtui in a supported terminal.",
    "example": "lunadashctl configure-network"
  },
  {
    "category": "Language, setup and maintenance",
    "name": "check-update",
    "value": "Empty",
    "behavior": "Ask the update checker to refresh; read status.update for the result.",
    "example": "lunadashctl check-update"
  },
  {
    "category": "Language, setup and maintenance",
    "name": "setup",
    "value": "Empty",
    "behavior": "Mark first-run setup incomplete and reopen the welcome flow.",
    "example": "lunadashctl setup"
  },
  {
    "category": "Language, setup and maintenance",
    "name": "finish-setup",
    "value": "Empty",
    "behavior": "Record first-run setup completion.",
    "example": "lunadashctl finish-setup"
  },
  {
    "category": "Language, setup and maintenance",
    "name": "quit",
    "value": "Literal \"confirm\"",
    "behavior": "Request normal LunaDash session shutdown. Empty is rejected.",
    "example": "lunadashctl quit confirm"
  }
];

export const statusFields: ApiField[] = [
  {
    "name": "clients",
    "type": "array",
    "purpose": "Window ids, metadata, geometry, workspace, focus/mapping and minimize/maximize/fullscreen state."
  },
  {
    "name": "workspace / tiling / windowLayout",
    "type": "number / objects",
    "purpose": "Current workspace, layout snapshot, template id, overlap policy, settings schema/values and actions."
  },
  {
    "name": "appearance",
    "type": "object",
    "purpose": "Effective desktop preferences."
  },
  {
    "name": "extensions",
    "type": "object",
    "purpose": "Installed plugins, remote Store catalogue, target registry, extensions document/path and Store state."
  },
  {
    "name": "settingsApi",
    "type": "object",
    "purpose": "Settings API version and editable target descriptors; same payload as settings-describe."
  },
  {
    "name": "shellModules",
    "type": "object",
    "purpose": "Module document/descriptors/path, custom-code trust and runtime errors."
  },
  {
    "name": "audio / network / power",
    "type": "objects",
    "purpose": "Service availability, devices/profiles, busy state and errors."
  },
  {
    "name": "display / brightness / ddcBrightness",
    "type": "objects",
    "purpose": "Output modes/scale/pending confirmation, backlight state and DDC/CI monitors."
  },
  {
    "name": "input / shortcuts",
    "type": "objects",
    "purpose": "Keyboard/backend/protocol state and effective shortcut map."
  },
  {
    "name": "screenCapture",
    "type": "object",
    "purpose": "zwlr_screencopy protocol/version, last path, busy phase and error."
  },
  {
    "name": "xwayland",
    "type": "object",
    "purpose": "XWayland compatibility and utility-surface state."
  },
  {
    "name": "system / update / sessionActions / settingsTools",
    "type": "objects",
    "purpose": "System state, updater, logind actions and validated settings helpers."
  },
  {
    "name": "language / translations",
    "type": "string / object",
    "purpose": "Locale and shell translation dictionary."
  },
  {
    "name": "settingsSerial / pickerSerial / launcherSerial",
    "type": "numbers",
    "purpose": "Interaction serials used by the shell to open UI surfaces."
  },
  {
    "name": "graphicsApi / shaderReady / graphicsFailed",
    "type": "string / booleans",
    "purpose": "Renderer-health summary for the active wlroots path."
  }
];

export const qmlApi: ApiField[] = [
  {
    "name": "shell.state",
    "type": "var",
    "purpose": "Latest status snapshot; treat it as read-only and mutate through commands."
  },
  {
    "name": "shell.command(method, value)",
    "type": "function",
    "purpose": "Queue a ludashctl operation. JSON-valued methods must receive JSON.stringify(...)."
  },
  {
    "name": "shell.commandCompleted(method, result)",
    "type": "signal",
    "purpose": "Emitted after a queued command returns with parsed JSON."
  },
  {
    "name": "shell.launch(id)",
    "type": "function",
    "purpose": "Launch terminal/files/browser defaults, open Settings/Plugins, or a built-in LunaDash desktop app."
  },
  {
    "name": "shell.openUrl(url)",
    "type": "function",
    "purpose": "Open a URL through the configured browser."
  },
  {
    "name": "shell.setAppearance(changes)",
    "type": "function",
    "purpose": "Convenience wrapper around the appearance control method."
  },
  {
    "name": "shell.openSettingsPage(page)",
    "type": "function",
    "purpose": "Open the shell Settings surface at a page id."
  },
  {
    "name": "shell.settingsOpen",
    "type": "bool",
    "purpose": "Current Settings visibility."
  },
  {
    "name": "shell.focusedTitle",
    "type": "string",
    "purpose": "Focused client title, or LunaDash when none is focused."
  },
  {
    "name": "shell.tr(source)",
    "type": "function",
    "purpose": "Translate with state.translations, falling back to source."
  },
  {
    "name": "shell.notify(title, body, kind, details)",
    "type": "function",
    "purpose": "Show shell feedback while respecting notification preferences."
  },
  {
    "name": "shell.configureNetwork()",
    "type": "function",
    "purpose": "Open the host network editor while preserving first-run behavior."
  }
];
