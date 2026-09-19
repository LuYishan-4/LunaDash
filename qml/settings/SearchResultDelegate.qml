import QtQuick
import QtQuick.Controls
import "../components"
import "../style"

ItemDelegate {
    id: result

    required property var entry
    required property var shell

    width: ListView.view ? ListView.view.width : implicitWidth
    height: 62
    hoverEnabled: true
    activeFocusOnTab: true
    scale: down ? 0.985 : hovered || activeFocus ? 1.008 : 1

    Accessible.role: Accessible.Button
    Accessible.name: shell.tr(entry.name) + ", " + shell.tr(entry.pageName)

    background: Rectangle {
        radius: 12
        color: result.down || result.highlighted
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.17)
            : result.hovered
                ? Theme.surfaceElevated
                : "transparent"
        border.width: result.activeFocus || result.highlighted ? 1 : 0
        border.color: result.activeFocus
            ? Theme.focusRing
            : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.42)

        Rectangle {
            visible: result.highlighted
            width: 3
            radius: 1.5
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            color: Theme.accent
        }

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }

    contentItem: Row {
        spacing: 12

        LineIcon {
            name: result.entry.page
            width: 19
            height: 19
            ink: result.highlighted || result.hovered ? Theme.accent : Theme.muted
            anchors.verticalCenter: parent.verticalCenter
            Behavior on ink { ColorAnimation { duration: Theme.motionFast } }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            width: Math.max(0, parent.width - 66)

            Text {
                width: parent.width
                text: result.shell.tr(result.entry.name)
                color: Theme.text
                font.family: Theme.font
                font.pixelSize: 13
                font.weight: result.highlighted ? Font.DemiBold : Font.Medium
                elide: Text.ElideRight
            }

            Text {
                width: parent.width
                text: result.shell.tr(result.entry.pageName)
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        LineIcon {
            width: 15
            height: 15
            anchors.verticalCenter: parent.verticalCenter
            name: "chevronRight"
            ink: Theme.muted
            opacity: result.hovered || result.highlighted || result.activeFocus ? 0.9 : 0.34
            transform: Translate {
                x: result.hovered || result.highlighted ? 3 : 0
            }
            Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
        }
    }

    Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
}
