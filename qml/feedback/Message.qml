import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: message
    moduleId: "feedback"
    anchors.top: true
    margins.top: Theme.barHeight + 10
    implicitWidth: moduleWidth(480)
    implicitHeight: moduleHeight(82)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-message"
    color: "transparent"
    contentItem.transform: Translate { y: (1 - message.reveal) * -18 }

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: Theme.surfaceOpaque
        border.width: 1
        border.color: Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.72)

        Rectangle {
            width: 4
            radius: 2
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 10
            color: Theme.danger
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 10
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 12

            LineIcon {
                width: 22
                height: 22
                name: "warning"
                ink: Theme.danger
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                Layout.fillWidth: true
                text: shell.tr(shell.errorMessage)
                wrapMode: Text.WordWrap
                color: moduleForeground
                font.family: Theme.font
                font.pixelSize: 12
                maximumLineCount: 3
                elide: Text.ElideRight
            }

            ShellButton {
                text: "×"
                quiet: true
                toolTip: shell.tr("Dismiss")
                onClicked: shell.errorMessage = ""
            }
        }
    }
}
