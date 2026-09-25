import QtQuick
import Quickshell.Io

Item {
    id: controller
    required property var shell
    property bool polling: true
    property var media: ({available: false, players: []})
    property string preferredService: ""
    property string preferredBus: ""
    property string errorMessage: ""
    property var pendingActions: []
    property int revision: 0
    property int statusRevision: 0

    function adopt(text, isAction) {
        if (!isAction && (statusRevision !== revision || actionProcess.running))
            return
        try {
            const result = JSON.parse(text)
            if (isAction && preferredService.length &&
                (result.service !== preferredService || result.bus !== preferredBus))
                return
            if (result.available !== undefined)
                media = result
            errorMessage = result.error || ""
            const players = result.players || []
            if (preferredService.length && !players.some(p => p.service === preferredService && p.bus === preferredBus)) {
                preferredService = ""
                preferredBus = ""
            }
        } catch (error) {
            errorMessage = shell.tr("Could not read the media player status.")
        }
    }
    function refresh() {
        if (!polling || statusProcess.running || actionProcess.running)
            return
        statusRevision = revision
        statusProcess.command = [shell.shellToolExecutable, "media-status", preferredService, preferredBus]
        statusProcess.running = true
    }
    function selectPlayer(service, bus) {
        preferredService = service
        preferredBus = bus
        // Disable commands until the newly selected player's snapshot arrives.
        media = Object.assign({}, media, {available: false, playing: false})
        ++revision
        refresh()
    }
    function run(action, value) {
        if (!media.available)
            return
        pendingActions = pendingActions.concat([[shell.shellToolExecutable, "media-action", action,
            String(media.service), String(media.bus), String(value === undefined ? "" : value), String(media.trackId || "")]])
        startAction()
    }
    function startAction() {
        if (actionProcess.running || !pendingActions.length)
            return
        ++revision
        actionProcess.command = pendingActions[0]
        pendingActions = pendingActions.slice(1)
        actionProcess.running = true
    }
    onPollingChanged: if (polling) refresh()
    Process {
        id: statusProcess
        stdout: StdioCollector { onStreamFinished: controller.adopt(text, false) }
        onExited: (code, status) => {
            if (code !== 0 && !controller.errorMessage.length)
                controller.errorMessage = controller.shell.tr("Could not read the media player status.")
            if (controller.statusRevision !== controller.revision)
                Qt.callLater(controller.refresh)
        }
    }
    Process {
        id: actionProcess
        stdout: StdioCollector { onStreamFinished: controller.adopt(text, true) }
        onExited: (code, status) => {
            Qt.callLater(controller.startAction)
            refreshTimer.restart()
        }
    }
    Timer {
        id: refreshTimer
        interval: 1500
        repeat: true
        running: controller.polling
        triggeredOnStart: true
        onTriggered: controller.refresh()
    }
}
