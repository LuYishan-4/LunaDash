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
        badge: page.audio.busy ? shell.tr("Applying…") : ""
        emphasized: page.audio.busy
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
                id: deviceRow
                required property string modelData
                // "data" would shadow the read-only Item.data property.
                readonly property var deviceInfo: page.audio[deviceRow.modelData] || ({})
                readonly property bool ready: Boolean(deviceRow.deviceInfo.available) && !page.audio.busy

                Layout.fillWidth: true
                spacing: 2

                SettingsComponents.SettingsSlider {
                    Layout.fillWidth: true
                    enabled: deviceRow.ready
                    label: shell.tr(deviceRow.modelData === "output" ? "Output volume" : "Microphone volume")
                    value: deviceRow.deviceInfo.volume || 0
                    minimum: 0
                    maximum: 100
                    step: 1
                    suffix: "%"
                    onMoved: value => shell.command("audio", JSON.stringify({device: deviceRow.modelData, volume: Math.round(value)}))
                }
                HelpText {
                    shell: page.shell
                    visible: !deviceRow.deviceInfo.available
                    message: "No default device is available. Check PipeWire and WirePlumber."
                }
                ShellButton {
                    Layout.alignment: Qt.AlignRight
                    iconName: "sound"
                    text: deviceRow.deviceInfo.muted ? shell.tr("Unmute") : shell.tr("Mute")
                    active: deviceRow.deviceInfo.muted ?? false
                    enabled: deviceRow.ready
                    Accessible.name: shell.tr(deviceRow.modelData === "output" ? "Mute output" : "Mute microphone")
                    onClicked: shell.command("audio", JSON.stringify({device: deviceRow.modelData, mute: !deviceRow.deviceInfo.muted}))
                }
            }
        }
    }

    HelpText { shell: page.shell; message: page.audio.error || "Volume changes affect the current system audio device. LunaDash limits gain to 100%." }
}
