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
    spacing: 16

    function parseBrightness(text) {
        const line = String(text || "").trim().split("\n")[0]
        const parts = line.split(",")
        if (parts.length < 4)
            return
        const percent = Number(String(parts[3]).replace("%", ""))
        if (Number.isFinite(percent)) {
            brightnessPercent = Math.max(0, Math.min(100, Math.round(percent)))
            brightnessAvailable = true
        }
    }

    function setBrightness(value) {
        if (brightnessSet.running)
            return
        brightnessSet.command = ["brightnessctl", "set", Math.round(value) + "%"]
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
            ? shell.tr("Adjust the active backlight device with brightnessctl.")
            : shell.tr("No writable backlight device was detected. Install brightnessctl or use the monitor controls.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Screen brightness"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            Text { text: brightnessAvailable ? brightnessPercent + "%" : "—"; color: Theme.muted; font.family: Theme.font }
        }
        SoftSlider {
            Layout.fillWidth: true
            from: 5
            to: 100
            stepSize: 1
            value: page.brightnessPercent
            enabled: page.brightnessAvailable && !brightnessSet.running
            onMoved: page.setBrightness(value)
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

    HelpText { shell: page.shell; message: "In a nested session, physical monitor resolution, refresh rate, scaling, rotation and night light are controlled by your host desktop. Brightness is adjustable when brightnessctl can access a backlight device." }
    HelpText { shell: page.shell; message: "Standalone multi-monitor configuration, HDR, color profiles and night light are not available yet." }

    Process {
        id: brightnessQuery
        command: ["brightnessctl", "-m"]
        stdout: StdioCollector { onStreamFinished: page.parseBrightness(text) }
        onExited: (code, status) => { if (code !== 0) page.brightnessAvailable = false }
    }
    Process {
        id: brightnessSet
        command: ["brightnessctl", "set", "50%"]
        onExited: (code, status) => {
            if (code === 0)
                brightnessQuery.running = true
        }
    }
    Component.onCompleted: brightnessQuery.running = true
}
