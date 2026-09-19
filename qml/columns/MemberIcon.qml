
import QtQuick
import QtQuick.Controls
import "../components"
import "../style"

Item {
    id: memberIcon
    required property var shell
    required property var member
    required property bool grouped
    readonly property var windowId: memberMouse.pressedWindow || memberIcon.member.window

    function resetGlyph() {
        glyph.x = 2
        glyph.y = 2
    }

    width: 30
    height: 30

    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["application/x-lunadash-window"]
        property bool invalidSource: false

        function hasWindowSource(event) {
            return event.source && event.source["windowId"] !== undefined && event.source["windowId"] !== null
        }

        function isSameWindow(event) {
            return hasWindowSource(event) && String(event.source["windowId"]) === String(memberIcon.windowId)
        }

        onEntered: drag => {
            invalidSource = isSameWindow(drag)
            const ok = hasWindowSource(drag) && !invalidSource
            if (ok) {
                drag.accept(Qt.MoveAction)
                memberIcon.shell.dropTarget = memberIcon.windowId
            } else {
                drag.accepted = false
            }
        }
        onPositionChanged: drag => {
            const ok = hasWindowSource(drag) && !isSameWindow(drag)
            if (ok) {
                drag.accept(Qt.MoveAction)
                memberIcon.shell.dropTarget = memberIcon.windowId
            } else {
                drag.accepted = false
            }
        }
        onExited: {
            invalidSource = false
            Qt.callLater(() => {
                if (memberIcon.shell.dropTarget === memberIcon.windowId)
                    memberIcon.shell.dropTarget = 0
            })
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: 10
        color: "transparent"
        border.width: dropArea.containsDrag ? 2 : 0
        border.color: dropArea.invalidSource ? Theme.danger : Theme.accent
        opacity: dropArea.containsDrag ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Math.min(Theme.motion, 120) } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion } }
    }

    Rectangle {
        id: glyph
        x: 2
        y: 2
        width: 26
        height: 26
        radius: 8
        z: memberMouse.drag.active ? 10 : 1
        color: memberIcon.member.focused
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.38)
            : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, memberIcon.member.minimized ? 0.07 : 0.16)
        border.width: memberIcon.member.focused ? 1 : 0
        border.color: Theme.accent
        opacity: memberIcon.member.minimized ? 0.58 : 1
        scale: memberMouse.pressed ? 0.92 : 1
        Drag.active: memberMouse.drag.active
        Drag.source: memberIcon
        Drag.keys: ["application/x-lunadash-window"]
        Drag.supportedActions: Qt.MoveAction
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2

        ApplicationIcon {
            shell: memberIcon.shell
            anchors.centerIn: parent
            width: 18
            height: 18
            iconName: String(memberIcon.member.icon || "")
            appId: String(memberIcon.member.appId || "")
            title: String(memberIcon.member.title || "")
        }

        Behavior on color { ColorAnimation { duration: Theme.motion } }
        Behavior on opacity { NumberAnimation { duration: Theme.motion } }
        Behavior on scale { NumberAnimation { duration: Math.min(Theme.motion, 100) } }
        Behavior on x { enabled: !memberMouse.drag.active; NumberAnimation { duration: Math.min(Theme.motion, 140); easing.type: Easing.OutCubic } }
        Behavior on y { enabled: !memberMouse.drag.active; NumberAnimation { duration: Math.min(Theme.motion, 140); easing.type: Easing.OutCubic } }
    }

    MouseArea {
        id: memberMouse
        objectName: "windowTaskButton"
        anchors.fill: parent
        z: 2
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: drag.active ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        drag.target: glyph
        drag.threshold: 6
        property var pressedWindow: 0
        property bool wasDragged: false
        onPressed: {
            pressedWindow = memberIcon.member.window
            wasDragged = false
        }
        onPositionChanged: { if (drag.active) wasDragged = true }
        onClicked: mouse => {
            // A status update may reorder or replace the item under the pointer.
            // Cancel that click instead of dispatching it to a different window.
            if (!wasDragged && pressedWindow && String(pressedWindow) === String(memberIcon.member.window))
                memberIcon.shell.command(mouse.button === Qt.RightButton ? "expel-window" : "focus", pressedWindow)
            pressedWindow = 0
        }
        onReleased: {
            const target = memberIcon.shell.dropTarget
            memberIcon.shell.dropTarget = 0
            if (wasDragged && target && pressedWindow && String(target) !== String(pressedWindow))
                memberIcon.shell.command("group-window", JSON.stringify({window: pressedWindow, target: target}))
            memberIcon.resetGlyph()
            Qt.callLater(() => { if (!memberMouse.pressed) memberMouse.pressedWindow = 0 })
        }
        onCanceled: {
            pressedWindow = 0
            memberIcon.shell.dropTarget = 0
            memberIcon.resetGlyph()
        }
    }

    Rectangle {
        visible: memberIcon.grouped && !memberMouse.drag.active
        anchors { top: parent.top; right: parent.right }
        width: 12
        height: 12
        radius: 6
        z: 3
        color: Theme.danger
        opacity: expelMouse.containsMouse ? 1 : 0.72
        Text { anchors.centerIn: parent; text: "−"; color: Theme.accentInk; font.pixelSize: 10; font.bold: true }
        MouseArea {
            id: expelMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            property var pressedWindow: 0
            onPressed: pressedWindow = memberIcon.member.window
            onClicked: {
                if (pressedWindow && String(pressedWindow) === String(memberIcon.member.window))
                    memberIcon.shell.command("expel-window", pressedWindow)
            }
        }
        ToolTip.visible: expelMouse.containsMouse
        ToolTip.delay: 350
        ToolTip.text: memberIcon.shell.tr("Expel from column")
        Behavior on opacity { NumberAnimation { duration: Theme.motion } }
    }

    ToolTip.visible: memberMouse.containsMouse && !memberMouse.drag.active
    ToolTip.delay: 450
    ToolTip.text: memberIcon.member.title || memberIcon.member.appId || memberIcon.shell.tr("Application window")
}
