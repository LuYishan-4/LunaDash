import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import "../components"
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    property var output: shell.state.display || ({})
    property bool brightnessAvailable: false
    property int brightnessPercent: 0
    property int pendingBrightness: -1
    spacing: 16

    function parseBrightness(text) {
        const line = String(text || "").trim().split("\n")[0]
        const parts = line.split(",")
        if (parts.length < 5) {
            brightnessAvailable = false
            return
        }
        const percentText = String(parts[parts.length - 1]).trim()
        if (!percentText.endsWith("%")) {
            brightnessAvailable = false
            return
        }
        const percent = Number(percentText.slice(0, -1))
        if (!Number.isFinite(percent)) {
            brightnessAvailable = false
            return
        }
        brightnessPercent = Math.max(0, Math.min(100, Math.round(percent)))
        brightnessAvailable = true
    }

    function scheduleBrightness(value) {
        pendingBrightness = Math.max(1, Math.min(100, Math.round(value)))
        brightnessPercent = pendingBrightness
        brightnessDebounce.restart()
    }

    function applyPendingBrightness() {
        if (pendingBrightness < 0 || brightnessSet.running)
            return
        const target = pendingBrightness
        pendingBrightness = -1
        brightnessSet.command = ["brightnessctl", "set", target + "%"]
        brightnessSet.running = true
    }

    PageTitle { shell: page.shell; title: "Display" }

    SettingsCard {
        title: shell.tr("Display information")
        description: (page.output.output || "") + "  ·  " + (page.output.width || 0) + " × " + (page.output.height || 0)
        Text { text: "Scale  " + (page.output.scale || 1) + "×   /   " + Math.round(page.output.refreshRate || 0) + " Hz"; color: Theme.muted; font.family: Theme.font }
    }

    SettingsCard {
        title: shell.tr("Brightness")
        description: brightnessAvailable
            ? shell.tr("Adjust the active backlight device.")
            : shell.tr("No controllable backlight device was detected. brightnessctl is required for laptop/internal-panel brightness control.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Screen brightness"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            Text { text: brightnessAvailable ? brightnessPercent + "%" : "—"; color: Theme.muted; font.family: Theme.font }
        }
        SoftSlider {
            Layout.fillWidth: true
            from: 1
            to: 100
            stepSize: 1
            value: page.brightnessPercent
            enabled: page.brightnessAvailable
            onMoved: page.scheduleBrightness(value)
        }
    }

    SettingsCard {
        title: shell.tr("Nested desktop size")
        description: shell.tr("Resize the LunaDash window while running as a nested compositor.")
        RowLayout {
            Repeater {
                model: ["1280x720", "1440x900", "1920x1080"]
                ShellButton { required property string modelData; text: modelData; enabled: page.output.nested && !page.output.fullscreen; onClicked: shell.command("desktop-size", modelData) }
            }
        }
    }

    HelpText { shell: page.shell; message: "In a nested session, physical monitor resolution, refresh rate, scaling, rotation and night light are controlled by your host desktop. Laptop/internal-panel brightness can still be adjusted when brightnessctl has access to a backlight device." }
    HelpText { shell: page.shell; message: "External monitors usually require DDC/CI rather than the kernel backlight interface, so they may not appear here." }

    Timer {
        id: brightnessDebounce
        interval: 90
        repeat: false
        onTriggered: page.applyPendingBrightness()
    }

    Process {
        id: brightnessQuery
        command: ["brightnessctl", "-m"]
        stdout: StdioCollector { onStreamFinished: page.parseBrightness(text) }
        onExited: (code, status) => {
            if (code !== 0)
                page.brightnessAvailable = false
        }
    }
    Process {
        id: brightnessSet
        command: ["brightnessctl", "set", "50%"]
        onExited: (code, status) => {
            if (code !== 0) {
                page.brightnessAvailable = false
                page.pendingBrightness = -1
                return
            }
            if (page.pendingBrightness >= 0)
                Qt.callLater(page.applyPendingBrightness)
            else
                brightnessQuery.running = true
        }
    }
    Component.onCompleted: brightnessQuery.running = true
}
