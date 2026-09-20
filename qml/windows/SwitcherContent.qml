import QtQuick
import QtQuick.Controls
import "../components"
import "../style"

Rectangle {
    id: selector
    required property var shell
    required property var selection
    readonly property var windows: selection.windows || []
    radius: 28
    color: Qt.rgba(Theme.background.r, Theme.background.g, Theme.background.b, 0.94)
    border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45)
    border.width: 1
    clip: true
    Text {
        x: 24; y: 20; width: parent.width - 48
        text: selector.shell.tr("Switch windows")
        color: Theme.text; font.family: Theme.font; font.pixelSize: 18; font.bold: true
        elide: Text.ElideRight
    }
    ListView {
        id: cards
        objectName: "windowSwitcherList"
        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: hint.top; margins: 20; topMargin: 58; bottomMargin: 12 }
        orientation: ListView.Horizontal
        spacing: 12
        clip: true
        model: selector.windows.length
        currentIndex: Number(selector.selection.index || 0)
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: Math.max(0, (width - 218) / 2)
        preferredHighlightEnd: preferredHighlightBegin + 218
        highlightMoveDuration: Theme.motion
        snapMode: ListView.SnapToItem
        boundsBehavior: Flickable.StopAtBounds
        onMovementEnded: {
            const target = selector.windows[currentIndex]
            if (target) selector.shell.command("switch-window", target.id)
        }
        delegate: Rectangle {
            id: card
            objectName: "windowSwitcherCard"
            required property int index
            readonly property var window: selector.windows[index] || ({})
            readonly property bool selected: index === Number(selector.selection.index || 0)
            width: Math.min(218, cards.width)
            height: cards.height
            radius: 18
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, selected ? 0.22 : 0.07)
            border.color: selected ? Theme.accent : "transparent"
            border.width: 2
            scale: selected ? 1 : 0.94
            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
            Behavior on scale { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }
            ApplicationIcon {
                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 22 }
                shell: selector.shell
                width: 52; height: 52
                appId: String(card.window.appId || "")
                iconName: String(card.window.icon || "")
                title: String(card.window.title || "")
            }
            Text {
                x: 16; y: 90; width: parent.width - 32; height: Math.max(24, parent.height - 128)
                text: card.window.title || card.window.appId || selector.shell.tr("Application window")
                color: Theme.text; font.family: Theme.font; font.pixelSize: 14
                wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
            }
            Text {
                anchors { bottom: parent.bottom; bottomMargin: 16; horizontalCenter: parent.horizontalCenter }
                text: selector.shell.tr("Workspace %1").arg(Number(card.window.workspace || 0) + 1)
                color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
            }
            MouseArea {
                objectName: "windowSwitcherButton"
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                property int pressedWindow: 0
                onPressed: pressedWindow = card.window.id
                onClicked: {
                    if (pressedWindow !== card.window.id) return
                    selector.shell.command("switch-window", pressedWindow)
                    selector.shell.command("switch-accept", "")
                }
            }
        }
    }
    Text {
        id: hint
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 20 }
        text: selector.shell.tr("Release Alt to switch · Esc to cancel")
        color: Theme.muted; font.family: Theme.font; font.pixelSize: 12
        horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap
    }
}
