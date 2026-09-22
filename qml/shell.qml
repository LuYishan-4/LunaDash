import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Services.Notifications
import "contextmenu"
import "wallpaper"
import "panel"
import "overview"
import "launcher"
import "settings"
import "session"
import "feedback"
import "setup"
import "style"
import "compatibility"
import "windows"
import "plugins"

ShellRoot {
    id: root
    // Package managers replace QML files individually. Keep the running shell
    // intact until the next login instead of loading a partial installation.
    Component.onCompleted: Quickshell.watchFiles = Quickshell.env("LUNADASH_QML_WATCH") === "1"
    property var state: ({ workspace: 0, clients: [], language: "en_US", wallpaper: 0, wallpaperImage: Quickshell.env("LUNADASH_WALLPAPER") || "" })
    property bool stateReady: false
    property string settingsPage: "general"
    property string lastCaptureError: ""
    readonly property string bin: Quickshell.env("LUNADASH_BIN_DIR") || Quickshell.env("LUDASH_BIN_DIR")
    readonly property string controlExecutable: bin ? bin + "/lunadashctl" : "lunadashctl"
    readonly property string desktopExecutable: bin ? bin + "/lunadash-desktop" : "lunadash-desktop"
    readonly property string updaterExecutable: bin ? bin + "/lunadash-update" : "lunadash-update"
    readonly property string shellToolExecutable: bin ? bin + "/lunadash-shell-tool" : "lunadash-shell-tool"
    readonly property string clipboardExecutable: bin ? bin + "/lunadash-clipboard-history" : "lunadash-clipboard-history"
    readonly property string assetDirectory: Quickshell.env("LUNADASH_ASSET_DIR")
    readonly property url iconSource: assetDirectory ? "file://" + assetDirectory + "/lunadash.png" : ""
    signal commandCompleted(string method, var result)
    property bool stopping: false
    property int lastSettingsSerial: 0
    property int lastPickerSerial: 0
    property int lastLauncherSerial: 0
    property int lastInteractionLauncherSerial: 0
    property int dropTarget: 0
    property bool launcherOpen: false
    property bool settingsOpen: false
    property bool pickerOpen: false
    property string pickerPurpose: "wallpaper"
    property string pendingWallpaper: ""
    property string wallpaperOverride: ""
    property bool calendarOpen: false
    property bool usbPopupOpen: false
    property bool volumePopupOpen: false
    property bool wifiPopupOpen: false
    property bool clipboardPopupOpen: false
    property var removableDevices: []
    property bool notificationVisible: false
    property bool notificationDetailsExpanded: false
    property var notification: ({title:"", body:"", kind:"info", details:"", actions:[]})
    property var activeExternalNotification: null
    property string notificationDetails: ""
    property int screenshotSerial: 0
    property string lastScreenshotSeen: ""
    property string stateFingerprint: ""
    property var updateInstall: ({
        state: "idle",
        channel: "",
        target: "",
        rollback: false,
        progress: 0,
        stage: "idle",
        message: "",
        details: ""
    })

    function notify(title, body, kind, details) {
        if (!((state.appearance || {}).notificationsEnabled ?? true)) return
        activeExternalNotification = null
        notificationDetailsExpanded = false
        notification = {title:String(title||"LunaDash"), body:String(body||""), kind:String(kind||"info"), details:String(details||""), actions:[]}
        notificationVisible = true
        notificationTimer.restart()
    }

    function clearNotification(explicitDismiss) {
        notificationVisible = false
        notificationDetailsExpanded = false
        if (activeExternalNotification) {
            if (explicitDismiss) activeExternalNotification.dismiss()
            else activeExternalNotification.expire()
            activeExternalNotification = null
        }
    }

    function setLanguage(locale) {
        if (!locale || languageAction.running)
            return
        languageAction.command = [shellToolExecutable, "language", String(locale)]
        languageAction.running = true
    }

    function installUpdate(channel, ref) {
        if (updateAction.running || !ref) return
        Quickshell.watchFiles = false
        updateAction.output = ""
        updateAction.errorOutput = ""
        updateInstall = {
            state: "running",
            channel: String(channel),
            target: String(ref),
            rollback: false,
            progress: 0,
            stage: "prepare",
            startedAt: Math.floor(Date.now() / 1000),
            updatedAt: Math.floor(Date.now() / 1000),
            message: "Preparing the update…",
            details: ""
        }
        updateAction.command = [updaterExecutable, String(channel), String(ref)]
        updateAction.running = true
        notify(tr("LunaDash update"), tr("Downloading and building the selected update…"), "info", "")
    }

    function rollbackUpdate() {
        if (updateAction.running) return
        Quickshell.watchFiles = false
        updateAction.output = ""
        updateAction.errorOutput = ""
        updateInstall = {
            state: "running",
            channel: "",
            target: "",
            rollback: true,
            progress: 0,
            stage: "rollback",
            startedAt: Math.floor(Date.now() / 1000),
            updatedAt: Math.floor(Date.now() / 1000),
            message: "Restoring the previous installation…",
            details: ""
        }
        updateAction.command = [updaterExecutable, "--rollback"]
        updateAction.running = true
        notify(tr("LunaDash rollback"), tr("Restoring the previous installation…"), "info", "")
    }

    NotificationServer {
        id: notificationServer
        bodySupported: true
        actionsSupported: true
        imageSupported: true
        persistenceSupported: true
        keepOnReload: true
        onNotification: incoming => {
            if (!((root.state.appearance || {}).notificationsEnabled ?? true)) {
                incoming.dismiss()
                return
            }
            incoming.tracked = true
            root.activeExternalNotification = incoming
            root.notificationDetailsExpanded = false
            const combined = (String(incoming.summary || "") + " " + String(incoming.body || "")).toLowerCase()
            const crash = combined.includes("crash") || combined.includes("segfault") || combined.includes("core dumped")
            if (crash && !((root.state.appearance || {}).crashNotifications ?? true)) {
                incoming.tracked = false
                root.activeExternalNotification = null
                return
            }
            const detail = crash
                ? [String(incoming.appName || ""), String(incoming.desktopEntry || ""), String(incoming.body || "")].filter(Boolean).join("\n")
                : ""
            root.notification = {
                title: String(incoming.summary || incoming.appName || root.tr("Notification")),
                body: String(incoming.body || ""),
                kind: crash ? "crash" : "info",
                details: detail,
                actions: incoming.actions || []
            }
            root.notificationVisible = true
            const requested = Number(incoming.expireTimeout || 0) * 1000
            notificationTimer.interval = requested > 0 ? Math.max(2500, Math.min(requested, 30000)) : 6500
            notificationTimer.restart()
        }
    }

    onLauncherOpenChanged: if (launcherOpen) { settingsOpen = false; calendarOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false; clipboardPopupOpen = false }
    onSettingsOpenChanged: if (settingsOpen) { launcherOpen = false; calendarOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false; clipboardPopupOpen = false } else { pickerOpen = false }
    onCalendarOpenChanged: if (calendarOpen) { usbPopupOpen = false; launcherOpen = false; volumePopupOpen = false; wifiPopupOpen = false; clipboardPopupOpen = false }
    onUsbPopupOpenChanged: if (usbPopupOpen) { calendarOpen = false; launcherOpen = false; volumePopupOpen = false; wifiPopupOpen = false; clipboardPopupOpen = false }
    onVolumePopupOpenChanged: if (volumePopupOpen) { calendarOpen = false; launcherOpen = false; usbPopupOpen = false; wifiPopupOpen = false; clipboardPopupOpen = false }
    onWifiPopupOpenChanged: if (wifiPopupOpen) { calendarOpen = false; launcherOpen = false; usbPopupOpen = false; volumePopupOpen = false; clipboardPopupOpen = false }
    onClipboardPopupOpenChanged: if (clipboardPopupOpen) { calendarOpen = false; launcherOpen = false; settingsOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false }

    property bool menuOpen: false
    property real menuX: 0
    property real menuY: 0
    function openMenu(x, y) { menuX = x; menuY = y; menuOpen = true }
    onMenuOpenChanged: if (menuOpen) { launcherOpen = false; settingsOpen = false; calendarOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false; clipboardPopupOpen = false }
    property bool x11Open: false
    readonly property bool overviewOpen: (state.appearance || {}).overview ?? false
    function setAppearance(changes) { command("appearance", JSON.stringify(changes)) }

    function syncPersistentUpdateInstall(result) {
        const persistent = (((result || {}).update || {}).install || null)
        if (!persistent)
            return

        const persistentState = String(persistent.state || "idle")
        const runtimeRunning = updateAction.running || root.updateInstall.state === "running"
        // When the user has just started an update, progress.json may still
        // contain the previous run for a few milliseconds. Never let that stale
        // idle/error/completed snapshot replace the live QML running state.
        if (updateAction.running && persistentState !== "running")
            return
        if (persistentState === "idle" && runtimeRunning)
            return
        if (updateAction.running && Number(persistent.updatedAt || 0) < Number(root.updateInstall.startedAt || 0))
            return
        const sameOperation = String(persistent.channel || "") === String(root.updateInstall.channel || "")
            && String(persistent.target || "") === String(root.updateInstall.target || "")
            && Boolean(persistent.rollback) === Boolean(root.updateInstall.rollback)

        root.updateInstall = {
            state: persistentState,
            channel: String(persistent.channel || ""),
            target: String(persistent.target || ""),
            rollback: Boolean(persistent.rollback),
            progress: Math.max(0, Math.min(100, Number(persistent.progress || 0))),
            stage: String(persistent.stage || "idle"),
            message: String(persistent.message || ""),
            details: sameOperation ? root.updateInstall.details || "" : "",
            startedAt: sameOperation ? root.updateInstall.startedAt || 0 : 0,
            updatedAt: Number(persistent.updatedAt || 0),
            lastUpdate: String(persistent.lastUpdate || "")
        }
    }

    function applyPolledState(result) {
        root.stateReady = true;
        // The compositor reads the updater's progress.json directly. Synchronize
        // install state before filtering high-frequency system metrics so About
        // keeps showing progress even after a QML reload.
        root.syncPersistentUpdateInstall(result)

        // System performance counters change every 1.5 s. Replacing the entire
        // root state object for those counters forces every settings binding to
        // re-evaluate while the user is typing. Keep the last system snapshot
        // unless a page that actually displays live performance is open.
        const needsLiveSystem = root.overviewOpen ||
            (root.settingsOpen && root.settingsPage === "about")
        if (!needsLiveSystem && root.state.system !== undefined)
            result.system = root.state.system

        const fingerprint = JSON.stringify(result)
        if (fingerprint === root.stateFingerprint)
            return
        root.stateFingerprint = fingerprint
        root.state = result
    }

    function openSettingsPage(page) {
        settingsPage = page
        if (settingsLoader.active) settingsLoader.item.showCategory(page)
        settingsOpen = true
    }
    onStateChanged: {
        if ((state.settingsSerial || 0) !== lastSettingsSerial) { lastSettingsSerial = state.settingsSerial; openSettingsPage(state.settingsPage || "general") }
        if ((state.pickerSerial || 0) !== lastPickerSerial) { lastPickerSerial = state.pickerSerial || 0; openSettingsPage("appearance"); pickerPurpose = "wallpaper"; settingsOpen = true; pickerOpen = true }
        if ((state.launcherSerial || 0) !== lastLauncherSerial) {
            lastLauncherSerial = state.launcherSerial || 0
            lastInteractionLauncherSerial = lastLauncherSerial
            launcherOpen = true
        }
        Theme.font = (state.appearance || {}).fontFamily || "sans-serif"
        Theme.clock24Hour = (state.appearance || {}).clock24Hour ?? true
        Theme.accent = (state.appearance || {}).accent || Theme.defaultAccent
        Theme.secondaryAccent = (state.appearance || {}).secondaryAccent || Theme.defaultSecondaryAccent
        Theme.barHeight = state.panelAtBottom ? 0 : (state.panelExtent ?? 40)
        Theme.animations = !stopping && ((state.appearance || {}).animations ?? true)
        Theme.animationDuration = state.shellAnimationDuration ?? (state.appearance || {}).animationDuration ?? 220
        const captureError = String((state.screenCapture || {}).error || "")
        if (captureError.length && captureError !== lastCaptureError)
            notify(tr("Screenshot failed"), tr(captureError), "error", captureError)
        lastCaptureError = captureError
        const capture = String((state.screenCapture || {}).lastCapture || "")
        if (capture.length && capture !== lastScreenshotSeen) {
            lastScreenshotSeen = capture
            screenshotSerial += 1
            notify(tr("Screenshot saved"), tr("Saved to") + " " + capture, "success", capture)
        }
        if (setupPaused) { const mapped = state.clients.some(client => client.mapped); if (mapped) setupEditorMapped = true; if ((!mapped && setupEditorMapped) || (!setupEditorMapped && ++setupWaitTicks >= 15)) setupPaused = false }
    }

    property bool logoutOpen: false
    property bool setupPaused: false
    property bool setupEditorMapped: false
    property int setupWaitTicks: 0
    function configureNetwork() { command("configure-network", ""); if (state.setupComplete === false) { setupPaused = true; setupEditorMapped = false; setupWaitTicks = 0 } }
    property string errorMessage: ""
    property string focusedTitle: (state.clients.find(client => client.focused) || {}).title || "LunaDash"
    function tr(source) { return (state.translations || {})[source] || source }
    function openUrl(url) { if (url) command("open-url", String(url)) }
    function command(method, value) { action.queue.push([method, String(value ?? "")]); dispatch() }
    function dispatch() { if (action.running || action.queue.length === 0) return; action.command = [controlExecutable].concat(action.queue.shift()); action.running = true }

    Process {
        id: action
        property var queue: []
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const result = JSON.parse(text)
                    const method = action.command[1]
                    if (result.error) {
                        if (method === "wallpaper-image" || method === "wallpaper-default")
                            root.wallpaperOverride = ""
                        root.errorMessage = root.tr(result.error)
                        root.notify(root.tr("System action failed"), root.tr(result.error), "error", result.error)
                    } else if (result.workspace !== undefined && result.clients !== undefined) {
                        // Most control actions already return a complete state
                        // snapshot. Apply it immediately so the UI does not need
                        // a high-frequency polling process to feel responsive.
                        root.applyPolledState(result)
                    }
                    root.commandCompleted(method, result)
                } catch(error) {
                    root.errorMessage = "Could not contact the desktop."
                }
            }
        }
        onExited: (exitCode, exitStatus) => { if (exitCode !== 0 && !root.errorMessage) root.errorMessage = "Could not contact the desktop."; Qt.callLater(root.dispatch) }
    }

    Process {
        id: clipboardWatcher
        command: [root.clipboardExecutable, "watch"]
        running: !root.stopping
        stderr: StdioCollector {
            onStreamFinished: if (text.trim())
                console.warn("Clipboard watcher:", text.trim())
        }
    }

    Process {
        id: languageAction
        command: [root.shellToolExecutable, "language", root.state.language || "en_US"]
        stderr: StdioCollector { onStreamFinished: if (text.trim()) root.notify(root.tr("Language"), text.trim(), "error", text.trim()) }
    }

    Process {
        id: updateAction
        property string output: ""
        property string errorOutput: ""
        stdout: StdioCollector { onStreamFinished: updateAction.output = text.trim() }
        stderr: StdioCollector { onStreamFinished: updateAction.errorOutput = text.trim() }
        onExited: (exitCode, exitStatus) => {
            const detail = [updateAction.errorOutput, updateAction.output].filter(Boolean).join("\n")
            const rollback = updateAction.command[1] === "--rollback"
            if (exitCode === 0) {
                root.updateInstall = {
                    state: "completed",
                    channel: rollback ? "" : String(updateAction.command[1] || ""),
                    target: rollback ? "" : String(updateAction.command[2] || ""),
                    rollback: rollback,
                    progress: 100,
                    stage: "complete",
                    startedAt: root.updateInstall.startedAt || 0,
                    updatedAt: Math.floor(Date.now() / 1000),
                    message: rollback ? "Rollback completed." : "Update installed successfully.",
                    details: detail
                }
                root.notify(root.tr(rollback ? "LunaDash rollback" : "LunaDash update"), root.tr(rollback ? "Rollback completed. Reboot to use the restored installation." : "Update completed. Reboot to start the new LunaDash installation."), "success", detail)
            } else {
                root.updateInstall = {
                    state: "error",
                    channel: rollback ? "" : String(updateAction.command[1] || ""),
                    target: rollback ? "" : String(updateAction.command[2] || ""),
                    rollback: rollback,
                    progress: Number(root.updateInstall.progress || 0),
                    stage: String(root.updateInstall.stage || "error"),
                    startedAt: root.updateInstall.startedAt || 0,
                    updatedAt: Math.floor(Date.now() / 1000),
                    message: rollback ? "Rollback failed." : "Update failed.",
                    details: detail
                }
                root.notify(root.tr("Update failed"), root.tr("The update could not be completed. Open details to view the log."), "error", detail)
            }
        }
    }

    function launch(id) {
        if (id === "terminal" || id === "files" || id === "browser") { command("launch-default", id); launcherOpen = false; return }
        if (id === "plugins") { openSettingsPage("plugins"); launcherOpen = false; return }
        if (id === "settings") { settingsOpen = true; launcherOpen = false; return }
        Quickshell.execDetached([desktopExecutable, "--app", id]); launcherOpen = false
    }

    Process {
        id: status
        command: [root.controlExecutable, "status"]
        stdout: StdioCollector {
            onStreamFinished: {
                if (!text.trim()) return
                try {
                    const result = JSON.parse(text)
                    if (result.error) return
                    root.applyPolledState(result)
                    if (root.wallpaperOverride.length) {
                        const actual = String(result.wallpaperImage || "")
                        const expected = root.wallpaperOverride.startsWith("file:") ? root.wallpaperOverride : "file://" + root.wallpaperOverride
                        if (actual === root.wallpaperOverride || actual === expected)
                            root.wallpaperOverride = ""
                    }
                    if (result.shutdown && !root.stopping) {
                        Theme.animations = false
                        root.stopping = true
                        shutdownTimer.start()
                    }
                } catch(error) {
                    console.warn(error)
                }
            }
        }
    }

    Timer { id: notificationTimer; interval: 6500; onTriggered: root.clearNotification(false) }
    Timer { id: shutdownTimer; interval: 100; repeat: true; onTriggered: if (root.state.layerSurfaces === 0 && !status.running && !action.running && !updateAction.running) Qt.quit() }
    Timer {
        interval: updateAction.running ? 350 : 1200
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: if (!status.running) status.running = true
    }

    property var interaction: ({})
    FileView {
        id: interactionFile
        path: (Quickshell.env("LUNADASH_CONTROL") || Quickshell.env("LUDASH_CONTROL")) + "-interaction.json"
        watchChanges: true
        onFileChanged: reload()
        onLoaded: {
            try {
                const next = JSON.parse(text())
                root.interaction = next
                const serial = Number(next.launcherSerial || 0)
                if (serial && serial !== root.lastInteractionLauncherSerial) {
                    root.lastInteractionLauncherSerial = serial
                    root.lastLauncherSerial = serial
                    root.launcherOpen = true
                }
            } catch (error) {
                root.interaction = ({})
            }
        }
    }
    PluginHost { shell: root }
    WindowFrames { shell: root; interaction: root.interaction }
    WindowSwitcher { shell: root; interaction: root.interaction }
    TilingDropHint { shell: root; drag: root.interaction.drag || ({}) }
    RemovableDeviceMonitor { shell: root }
    StartupSplash { shell: root; ready: root.stateReady && desktopWallpaper.ready }
    Wallpaper { id: desktopWallpaper; shell: root; opened: !root.stopping }
    TopPanel { shell: root; opened: !root.stopping }
    LazyLoader {
        loading: root.overviewOpen
        Overview { shell: root; opened: !root.stopping && root.overviewOpen }
    }
    LazyLoader {
        loading: root.calendarOpen
        CalendarPopup { shell: root; opened: !root.stopping && root.calendarOpen }
    }
    LazyLoader {
        loading: root.usbPopupOpen
        UsbDevicePopup { shell: root; opened: !root.stopping && root.usbPopupOpen }
    }
    LazyLoader {
        loading: root.volumePopupOpen
        AudioPopup { shell: root; opened: !root.stopping && root.volumePopupOpen }
    }
    LazyLoader {
        loading: root.clipboardPopupOpen
        ClipboardPopup { shell: root; opened: !root.stopping && root.clipboardPopupOpen }
    }
    LazyLoader {
        loading: root.wifiPopupOpen
        WifiPopup { shell: root; opened: !root.stopping && root.wifiPopupOpen }
    }
    LazyLoader {
        loading: root.launcherOpen
        Launcher { shell: root; opened: !root.stopping && root.launcherOpen }
    }
    DesktopMenu { shell: root; opened: !root.stopping && root.menuOpen; anchorX: root.menuX; anchorY: root.menuY }
    LazyLoader {
        id: settingsLoader
        loading: root.settingsOpen
        onActiveChanged: if (active) item.showCategory(root.settingsPage)
        SettingsPanel {
            shell: root
            opened: !root.stopping && root.settingsOpen
            onCategoryChanged: if (settingsLoader.active) root.settingsPage = category
        }
    }
    SetupWizard { shell: root; opened: !root.stopping && root.state.setupComplete === false && !root.setupPaused }
    LazyLoader {
        loading: root.logoutOpen
        LogoutPanel { shell: root; opened: !root.stopping && root.logoutOpen }
    }
    LazyLoader {
        loading: root.x11Open
        X11Launcher { shell: root; opened: !root.stopping && root.x11Open }
    }
    Message { shell: root; opened: !root.stopping && root.errorMessage.length > 0 }
    NotificationToast { shell: root; opened: !root.stopping && root.notificationVisible }
    ScreenshotFeedback { shell: root; opened: !root.stopping }
    WorkspaceTransition { shell: root; opened: !root.stopping }
}
