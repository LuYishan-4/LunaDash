import QtQuick
import QtQuick.Controls
import "../components"
import "../style"

ItemDelegate {
    id: result
    required property var entry
    required property var shell
    width: ListView.view ? ListView.view.width : implicitWidth
    height: 58
    hoverEnabled: true
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: shell.tr(entry.name) + ", " + shell.tr(entry.pageName)
    background: Rectangle {
        radius: 10
        color: result.down || result.highlighted ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.16) : result.hovered ? Theme.surface : "transparent"
        border.width: result.activeFocus ? 1 : 0
        border.color: Theme.accent
    }
    contentItem: Row {
        spacing: 12
        LineIcon { name: result.entry.page; width: 19; height: 19; ink: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text { text: result.shell.tr(result.entry.name); color: Theme.text; font.family: Theme.font; font.pixelSize: 13 }
            Text { text: result.shell.tr(result.entry.pageName); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11 }
        }
    }
}
