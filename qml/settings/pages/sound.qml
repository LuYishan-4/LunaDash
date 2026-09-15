import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell

    readonly property var audio: shell.state.audio || ({})

    spacing: 16

    PageTitle { shell: page.shell; title: "Sound" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Playback device")
        description: shell.tr("Choose the PipeWire output used by the desktop.")
        StyledComboBox {
            Layout.fillWidth: true
            model: page.audio.outputDevices || []
            textRole: "name"
            currentIndex: Math.max(0, model.findIndex(device => device.default))
            enabled: model.length > 0 && !page.audio.busy
            onActivated: index => shell.command("audio", JSON.stringify({device: "output", id: model[index].id}))
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Volume")
        description: shell.tr("Adjust output and microphone levels for the current session.")
        Repeater {
            model: ["output", "input"]
            ColumnLayout {
                id: device
                required property string modelData
                readonly property var data: (page.audio)[device.modelData] || ({})
                readonly property bool ready: Boolean(device.data.available) && !page.audio.busy

                Layout.fillWidth: true
                spacing: 2

                SettingsComponents.SettingsSlider {
                    Layout.fillWidth: true
                    enabled: device.ready
                    label: shell.tr(device.modelData === "output" ? "Output volume" : "Microphone volume")
                    value: device.data.volume || 0
                    minimum: 0
                    maximum: 100
                    step: 1
                    suffix: "%"
                    onMoved: value => shell.command("audio", JSON.stringify({device: device.modelData, volume: Math.round(value)}))
                }
                HelpText {
                    shell: page.shell
                    visible: !device.data.available
                    message: "No default device is available. Check PipeWire and WirePlumber."
                }
                ShellButton {
                    Layout.alignment: Qt.AlignRight
                    text: device.data.muted ? shell.tr("Unmute") : shell.tr("Mute")
                    active: device.data.muted ?? false
                    enabled: device.ready
                    Accessible.name: shell.tr(device.modelData === "output" ? "Mute output" : "Mute microphone")
                    onClicked: shell.command("audio", JSON.stringify({device: device.modelData, mute: !device.data.muted}))
                }
            }
        }
    }

    HelpText { shell: page.shell; message: page.audio.error || "Volume changes affect the current system audio device. LunaDash limits gain to 100%." }
}
