import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: toast
    moduleId: "notification"
    anchors.right: true
    anchors.bottom: true
    margins.right: 22
    margins.bottom: 22
    implicitWidth: moduleWidth(420)
    implicitHeight: moduleHeight(shell.notificationDetailsExpanded ? 190 : (shell.notification.details || (shell.notification.actions || []).length ? 138 : 96))
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-notification"
    color: "transparent"
    contentItem.opacity: reveal
    contentItem.transform: Translate { x: (1 - toast.reveal) * 48 }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: moduleBackground
        border.width: 1
        border.color: shell.notification.kind === "crash" || shell.notification.kind === "error" ? Theme.danger : Theme.border
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 14; spacing: 8
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                LineIcon { width: 24; height: 24; name: shell.notification.kind === "crash" || shell.notification.kind === "error" ? "warning" : "info"; ink: shell.notification.kind === "crash" || shell.notification.kind === "error" ? Theme.danger : Theme.accent }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { Layout.fillWidth:true; text:shell.notification.title||"LunaDash"; color:Theme.text; font.family:Theme.font; font.pixelSize:13; font.bold:true; elide:Text.ElideRight }
                    Text { Layout.fillWidth:true; text:shell.notification.body||""; color:Theme.muted; font.family:Theme.font; font.pixelSize:11; textFormat:Text.PlainText; wrapMode:Text.WordWrap; maximumLineCount:shell.notificationDetailsExpanded?5:2; elide:Text.ElideRight }
                }
                ShellButton { text:"×"; onClicked:shell.clearNotification(true) }
            }
            Text {
                visible: shell.notificationDetailsExpanded && Boolean(shell.notification.details)
                Layout.fillWidth: true
                text: shell.notification.details || ""
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 10
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                maximumLineCount: 5
                elide: Text.ElideRight
            }
            RowLayout {
                visible: Boolean(shell.notification.details) || (shell.notification.actions || []).length > 0
                Layout.fillWidth: true; spacing: 6
                ShellButton {
                    visible: Boolean(shell.notification.details)
                    text: shell.notificationDetailsExpanded ? shell.tr("Hide details") : shell.tr("View details")
                    onClicked: shell.notificationDetailsExpanded = !shell.notificationDetailsExpanded
                }
                Repeater {
                    model: shell.notification.actions || []
                    ShellButton {
                        required property var modelData
                        text: modelData.text || shell.tr("Open")
                        active: true
                        onClicked: { modelData.invoke(); shell.clearNotification(false) }
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
    }
}
