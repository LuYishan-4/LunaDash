import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    spacing: 22
    PageTitle { shell: page.shell; title: "Sound" }
    Repeater {
        model: ["output", "input"]
        ColumnLayout {
            id: device
            required property string modelData
            property var data: (shell.state.audio || {})[modelData] || ({})
            Layout.fillWidth: true
            Text { text: shell.tr(device.modelData === "output" ? "Output volume" : "Microphone volume"); color: Theme.text; font.pixelSize: 17 }
            HelpText { shell: page.shell; visible: !device.data.available; message: "No default device is available. Check PipeWire and WirePlumber, or open the audio device settings below." }
            RowLayout {
                enabled: Boolean(device.data.available) && !(shell.state.audio || {}).busy
                SoftSlider {
                    id: level; Layout.fillWidth: true; from: 0; to: 100; stepSize: 1
                    Binding on value { value: device.data.volume || 0; when: !level.pressed }
                    Accessible.name: shell.tr(device.modelData === "output" ? "Output volume" : "Microphone volume")
                    onPressedChanged: if (!pressed) shell.command("audio", JSON.stringify({device: device.modelData, volume: Math.round(value)}))
                    Keys.onReleased: event => { if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) shell.command("audio", JSON.stringify({device: device.modelData, volume: Math.round(value)})) }
                }
                Text { text: Math.round(level.value) + "%"; color: Theme.accent }
                ShellButton { text: shell.tr("Mute"); active: device.data.muted ?? false; onClicked: shell.command("audio", JSON.stringify({device: device.modelData, mute: !device.data.muted})) }
            }
        }
    }
    HelpText { shell: page.shell; message: (shell.state.audio || {}).error || "Volume changes affect the current system audio device. LunaDah limits gain to 100%." }
    ToolList { shell: page.shell; category: "sound" }
}
