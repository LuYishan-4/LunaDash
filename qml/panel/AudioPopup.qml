import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: popup
    moduleId: "overview"
    anchors.top: true
    anchors.right: true
    margins.top: Theme.barHeight + 8
    margins.right: 10
    implicitWidth: moduleWidth(360)
    implicitHeight: moduleHeight(250)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-audio-popup"
    color: "transparent"

    readonly property var audio: shell.state.audio || ({})
    property real reveal: opened ? 1 : 0
    Behavior on reveal { NumberAnimation { duration: Math.max(140, Theme.motion); easing.type: Easing.OutCubic } }

    Rectangle {
        anchors.fill: parent
        radius: 22
        color: Theme.surfaceOpaque
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.38)
        opacity: popup.reveal
        scale: 0.94 + 0.06 * popup.reveal
        transformOrigin: Item.TopRight
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12
        opacity: popup.reveal

        RowLayout {
            Layout.fillWidth: true
            LineIcon { width: 20; height: 20; name: "sound"; ink: Theme.accent }
            Text { Layout.fillWidth: true; text: shell.tr("Sound"); color: Theme.text; font.family: Theme.font; font.pixelSize: 18; font.weight: Font.DemiBold }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close"); onClicked: shell.volumePopupOpen = false }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text { text: shell.tr("Output volume") + "  " + Math.round((popup.audio.output || {}).volume || 0) + "%"; color: Theme.text; font.family: Theme.font }
            SoftSlider {
                Layout.fillWidth: true
                from: 0
                to: 100
                stepSize: 1
                value: (popup.audio.output || {}).volume || 0
                enabled: Boolean((popup.audio.output || {}).available) && !popup.audio.busy
                onMoved: shell.command("audio", JSON.stringify({device:"output", volume:Math.round(value)}))
            }
            ShellButton {
                Layout.alignment: Qt.AlignRight
                text: (popup.audio.output || {}).muted ? shell.tr("Unmute") : shell.tr("Mute")
                active: (popup.audio.output || {}).muted ?? false
                onClicked: shell.command("audio", JSON.stringify({device:"output", mute:!((popup.audio.output || {}).muted ?? false)}))
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth: true; text: shell.tr("Audio devices and routing"); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
            ShellButton { text: shell.tr("More"); onClicked: { shell.volumePopupOpen = false; shell.command("open-settings", "sound") } }
        }
    }
}
