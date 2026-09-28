import QtQuick
import Quickshell.Io

// Own this controller at shell scope: closing the gallery must not cancel a
// pending favorite write. Paths are separate argv entries, never shell source.
Item {
    id: controller
    required property var shell
    property var pending: []
    property bool responseReceived: false
    readonly property bool busy: process.running || pending.length > 0
    signal updated
    function run(action, path, favorite) {
        if (!["refresh", "create-directory", "favorite"].includes(action)) return
        const args = [shell.shellToolExecutable, "wallpaper", action]
        if (action === "favorite") args.push(String(path), favorite ? "true" : "false")
        if (pending.length < 64) pending = pending.concat([args])
        startNext()
    }
    function startNext() {
        if (process.running || !pending.length) return
        responseReceived = false
        process.command = pending[0]
        pending = pending.slice(1)
        process.running = true
    }
    Process {
        id: process
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const result = JSON.parse(text)
                    controller.responseReceived = true
                    if (result.error)
                        controller.shell.notify(controller.shell.tr("Wallpaper library"),
                            controller.shell.tr(result.error), "error", "")
                } catch (error) {
                    controller.responseReceived = false
                }
            }
        }
        onExited: (code, status) => {
            if (!controller.responseReceived)
                controller.shell.notify(controller.shell.tr("Wallpaper library"),
                    controller.shell.tr("Could not update the wallpaper library."), "error", "")
            controller.updated()
            Qt.callLater(controller.startNext)
        }
    }
}
