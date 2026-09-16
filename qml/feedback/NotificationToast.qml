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
    implicitWidth: moduleWidth(390)
    implicitHeight: moduleHeight(shell.notification.details ? 126 : 96)
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
        border.color: shell.notification.kind === "crash" ? Theme.danger : Theme.border
        RowLayout {
            anchors.fill: parent; anchors.margins: 14; spacing: 12
            LineIcon { width: 24; height: 24; name: shell.notification.kind === "crash" ? "warning" : "info"; ink: shell.notification.kind === "crash" ? Theme.danger : Theme.accent }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 4
                Text { Layout.fillWidth:true; text:shell.notification.title||"LunaDash"; color:Theme.text; font.family:Theme.font; font.pixelSize:13; font.bold:true; elide:Text.ElideRight }
                Text { Layout.fillWidth:true; text:shell.notification.body||""; color:Theme.muted; font.family:Theme.font; font.pixelSize:11; wrapMode:Text.WordWrap; maximumLineCount:2; elide:Text.ElideRight }
                ShellButton { visible:Boolean(shell.notification.details); text:shell.tr("View details"); onClicked:{ shell.notificationDetails=shell.notification.details; shell.settingsOpen=true; shell.notificationVisible=false } }
            }
            ShellButton { text:"×"; onClicked:shell.notificationVisible=false }
        }
    }
}
