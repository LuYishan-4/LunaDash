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
    Behavior on implicitHeight { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-notification"
    color: "transparent"
    readonly property color kindColor: shell.notification.kind === "crash" || shell.notification.kind === "error"
        ? Theme.danger
        : shell.notification.kind === "success"
            ? Theme.success
            : Theme.accent
    contentItem.opacity: reveal
    contentItem.transform: Translate { x: (1 - toast.reveal) * 48 }

    Rectangle {
        anchors.fill: parent
        radius: 16
        color: moduleBackground
        border.width: 1
        border.color: Qt.rgba(toast.kindColor.r, toast.kindColor.g, toast.kindColor.b, 0.68)

        Rectangle {
            width: 4
            radius: 2
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 10
            color: toast.kindColor
        }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 14; spacing: 8
            RowLayout {
                Layout.fillWidth: true; spacing: 12
                Rectangle {
                    width: 34
                    height: 34
                    radius: 17
                    color: Qt.rgba(toast.kindColor.r, toast.kindColor.g, toast.kindColor.b, 0.14)
                    LineIcon {
                        anchors.centerIn: parent
                        width: 20
                        height: 20
                        name: shell.notification.kind === "crash" || shell.notification.kind === "error" ? "warning" : shell.notification.kind === "success" ? "check" : "info"
                        ink: toast.kindColor
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { Layout.fillWidth:true; text:shell.notification.title||"LunaDash"; color:Theme.text; font.family:Theme.font; font.pixelSize:13; font.bold:true; elide:Text.ElideRight }
                    Text { Layout.fillWidth:true; text:shell.notification.body||""; color:Theme.muted; font.family:Theme.font; font.pixelSize:11; textFormat:Text.PlainText; wrapMode:Text.WordWrap; maximumLineCount:shell.notificationDetailsExpanded?5:2; elide:Text.ElideRight }
                }
                ShellButton { text:"×"; quiet:true; toolTip:shell.tr("Dismiss"); onClicked:shell.clearNotification(true) }
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
