import QtQuick
import QtQuick.Controls
import "../components"
import "../style"

Item {
    id: memberIcon
    required property var shell
    required property var member
    width: 30
    height: 30
    Rectangle {
        anchors.fill: parent
        anchors.margins: 2
        radius: 8
        color: memberIcon.member.focused
            ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.38)
            : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, memberMouse.containsMouse ? 0.24 : 0)
        opacity: memberIcon.member.minimized ? 0.58 : 1
        scale: memberMouse.pressed ? 0.92 : 1
        ApplicationIcon {
            shell: memberIcon.shell
            anchors.centerIn: parent
            width: 18
            height: 18
            iconName: String(memberIcon.member.icon || "")
            appId: String(memberIcon.member.appId || "")
            title: String(memberIcon.member.title || "")
        }
        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        Behavior on scale { NumberAnimation { duration: Theme.motionFast } }
    }
    MouseArea {
        id: memberMouse
        objectName: "windowTaskButton"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        property var pressedWindow: 0
        onPressed: pressedWindow = memberIcon.member.window
        onClicked: {
            // Polling must never redirect a held click to a replacement client.
            if (pressedWindow && String(pressedWindow) === String(memberIcon.member.window))
                memberIcon.shell.command("activate-window", pressedWindow)
            pressedWindow = 0
        }
        onCanceled: pressedWindow = 0
    }
    ToolTip.visible: memberMouse.containsMouse
    ToolTip.delay: 450
    ToolTip.text: memberIcon.member.title || memberIcon.member.appId || memberIcon.shell.tr("Application window")
}
