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
import "startup"

ShellRoot {
    id: root
    property var state: ({ workspace: 0, clients: [], language: "en_US", wallpaper: 0 })
    readonly property string bin: Quickshell.env("LUNADASH_BIN_DIR") || Quickshell.env("LUDASH_BIN_DIR")
    readonly property string controlExecutable: bin ? bin + "/lunadashctl" : "lunadashctl"
    readonly property string desktopExecutable: bin ? bin + "/lunadash-desktop" : "lunadash-desktop"
    readonly property string updaterExecutable: bin ? bin + "/lunadash-update" : "lunadash-update"
    readonly property string shellToolExecutable: bin ? bin + "/lunadash-shell-tool" : "lunadash-shell-tool"
    readonly property string assetDirectory: Quickshell.env("LUNADASH_ASSET_DIR")
    readonly property url iconSource: assetDirectory ? "file://" + assetDirectory + "/lunadash.png" : ""
    signal commandCompleted(string method, var result)
    property bool stopping: false
    property int lastSettingsSerial: 0
    property int lastPickerSerial: 0
    property int dropTarget: 0
    property bool launcherOpen: false
    property bool settingsOpen: false
    property bool pickerOpen: false
    property string pendingWallpaper: ""
    property bool calendarOpen: false
    property bool usbPopupOpen: false
    property bool volumePopupOpen: false
    property bool wifiPopupOpen: false
    property var removableDevices: []
    property bool notificationVisible: false
    property bool notificationDetailsExpanded: false
    property var notification: ({title:"", body:"", kind:"info", details:"", actions:[]})
    property var activeExternalNotification: null
    property string notificationDetails: ""

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
        updateAction.output = ""
        updateAction.errorOutput = ""
        updateAction.command = [updaterExecutable, String(channel), String(ref)]
        updateAction.running = true
        notify(tr("LunaDash update"), tr("Downloading and building the selected update…"), "info", "")
    }

    function rollbackUpdate() {
        if (updateAction.running) return
        updateAction.output = ""
        updateAction.errorOutput = ""
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

    onLauncherOpenChanged: if (launcherOpen) { settingsOpen = false; calendarOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false }
    onSettingsOpenChanged: if (settingsOpen) { launcherOpen = false; calendarOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false } else { pickerOpen = false }
    onCalendarOpenChanged: if (calendarOpen) { usbPopupOpen = false; launcherOpen = false; volumePopupOpen = false; wifiPopupOpen = false }
    onUsbPopupOpenChanged: if (usbPopupOpen) { calendarOpen = false; launcherOpen = false; volumePopupOpen = false; wifiPopupOpen = false }
    onVolumePopupOpenChanged: if (volumePopupOpen) { calendarOpen = false; launcherOpen = false; usbPopupOpen = false; wifiPopupOpen = false }
    onWifiPopupOpenChanged: if (wifiPopupOpen) { calendarOpen = false; launcherOpen = false; usbPopupOpen = false; volumePopupOpen = false }

    property bool menuOpen: false
    property real menuX: 0
    property real menuY: 0
    function openMenu(x, y) { menuX = x; menuY = y; menuOpen = true }
    onMenuOpenChanged: if (menuOpen) { launcherOpen = false; settingsOpen = false; calendarOpen = false; usbPopupOpen = false; volumePopupOpen = false; wifiPopupOpen = false }
    property bool x11Open: false
    property bool startupLogoVisible: true
    readonly property bool overviewOpen: (state.appearance || {}).overview ?? false
    function setAppearance(changes) { command("appearance", JSON.stringify(changes)) }

    onStateChanged: {
        if ((state.settingsSerial || 0) !== lastSettingsSerial) { lastSettingsSerial = state.settingsSerial; settingsCenter.showCategory(state.settingsPage || "general"); settingsOpen = true }
        if ((state.pickerSerial || 0) !== lastPickerSerial) { lastPickerSerial = state.pickerSerial || 0; settingsCenter.showCategory("appearance"); settingsOpen = true; pickerOpen = true }
        Theme.font = (state.appearance || {}).fontFamily || "sans-serif"
        Theme.clock24Hour = (state.appearance || {}).clock24Hour ?? true
        Theme.accent = (state.appearance || {}).accent || Theme.defaultAccent
        Theme.secondaryAccent = (state.appearance || {}).secondaryAccent || Theme.defaultSecondaryAccent
        Theme.barHeight = state.panelAtBottom ? 0 : (state.panelExtent ?? 40)
        Theme.animations = !stopping && ((state.appearance || {}).animations ?? true)
        Theme.animationDuration = (state.appearance || {}).animationDuration ?? 220
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
    function command(method, value) { action.queue.push([method, String(value ?? "")]); dispatch() }
    function dispatch() { if (action.running || action.queue.length === 0) return; action.command = [controlExecutable].concat(action.queue.shift()); action.running = true }

    Process {
        id: action
        property var queue: []
        stdout: StdioCollector { onStreamFinished: { try { const result=JSON.parse(text); if(result.error){root.errorMessage=root.tr(result.error);root.notify(root.tr("System action failed"),root.tr(result.error),"error",result.error)} root.commandCompleted(action.command[1],result) } catch(error){root.errorMessage="Could not contact the desktop."} } }
        onExited: (exitCode, exitStatus) => { if (exitCode !== 0 && !root.errorMessage) root.errorMessage = "Could not contact the desktop."; Qt.callLater(root.dispatch) }
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
            if (exitCode === 0)
                root.notify(root.tr(rollback ? "LunaDash rollback" : "LunaDash update"), root.tr(rollback ? "Rollback completed. Start a new session to use it." : "Update completed. Start a new session to use it."), "success", detail)
            else
                root.notify(root.tr("Update failed"), root.tr("The update could not be completed. Open details to view the log."), "error", detail)
        }
    }

    function launch(id) {
        if (id === "terminal" || id === "files") { command("launch-default", id); launcherOpen = false; return }
        if (id === "settings") { settingsOpen = true; launcherOpen = false; return }
        Quickshell.execDetached([desktopExecutable, "--app", id]); launcherOpen = false
    }

    Process {
        id: status
        command: [root.controlExecutable, "status"]
        stdout: StdioCollector { onStreamFinished: { if(!text.trim())return;try{const result=JSON.parse(text);if(result.error)return;root.state=result;if(result.shutdown&&!root.stopping){Theme.animations=false;root.stopping=true;shutdownTimer.start()}}catch(error){console.warn(error)} } }
    }

    Timer { id: notificationTimer; interval: 6500; onTriggered: root.clearNotification(false) }
    Timer { id: shutdownTimer; interval: 100; repeat: true; onTriggered: if (root.state.layerSurfaces === 0 && !status.running && !action.running && !updateAction.running) Qt.quit() }
    Timer { interval: 700; running: true; repeat: true; triggeredOnStart: true; onTriggered: if (!status.running) status.running = true }

    RemovableDeviceMonitor { shell: root }
    Wallpaper { shell: root; opened: !root.stopping }
    TopPanel { shell: root; opened: !root.stopping }
    Overview { shell: root; opened: !root.stopping && root.overviewOpen }
    CalendarPopup { shell: root; opened: !root.stopping && root.calendarOpen }
    UsbDevicePopup { shell: root; opened: !root.stopping && root.usbPopupOpen }
    AudioPopup { shell: root; opened: !root.stopping && root.volumePopupOpen }
    WifiPopup { shell: root; opened: !root.stopping && root.wifiPopupOpen }
    Launcher { shell: root; opened: !root.stopping && root.launcherOpen }
    DesktopMenu { shell: root; opened: !root.stopping && root.menuOpen; anchorX: root.menuX; anchorY: root.menuY }
    SettingsPanel { id: settingsCenter; shell: root; opened: !root.stopping && root.settingsOpen }
    SetupWizard { shell: root; opened: !root.stopping && root.state.setupComplete === false && !root.setupPaused }
    LogoutPanel { shell: root; opened: !root.stopping && root.logoutOpen }
    X11Launcher { shell: root; opened: !root.stopping && root.x11Open }
    Message { shell: root; opened: !root.stopping && root.errorMessage.length > 0 }
    NotificationToast { shell: root; opened: !root.stopping && root.notificationVisible }
    WorkspaceTransition { shell: root; opened: !root.stopping }
    StartupLogoOverlay { shell: root; opened: !root.stopping && root.startupLogoVisible; onFinished: root.startupLogoVisible = false }
}
