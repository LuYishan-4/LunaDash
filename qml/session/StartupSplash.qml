import QtQuick
import QtQuick.Window
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"

PanelWindow {
    id: splash
    required property var shell
    property bool ready: false
    property bool minimumElapsed: false
    property bool firstFrameSeen: false
    property bool finished: false
    property bool delayed: false
    property real reveal: 1
    visible: !finished || reveal > 0
    anchors { top: true; bottom: true; left: true; right: true }
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-startup"
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    color: "transparent"

    function finishIfReady() {
        if (ready && minimumElapsed && !finished) {
            finished = true
            reveal = 0
        }
    }
    onReadyChanged: finishIfReady()
    Connections {
        target: splash.contentItem.Window.window
        function onFrameSwapped() {
            if (!splash.firstFrameSeen) {
                splash.firstFrameSeen = true
                minimumTimer.start()
            }
        }
    }
    Timer { id: minimumTimer; interval: 650; onTriggered: { splash.minimumElapsed = true; splash.finishIfReady() } }
    Timer { interval: 10000; running: !splash.finished; onTriggered: splash.delayed = true }

    Rectangle {
        anchors.fill: parent
        color: "#0b1114"
        opacity: splash.reveal
        Column {
            anchors.centerIn: parent
            width: Math.min(360, parent.width - 48)
            spacing: 24
            LunaDashLogo {
                id: logo
                width: 112; height: 112
                anchors.horizontalCenter: parent.horizontalCenter
                SequentialAnimation on opacity {
                    running: !splash.finished && Theme.animations
                    loops: Animation.Infinite
                    NumberAnimation { from: 0.5; to: 1; duration: 650; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 1; to: 0.5; duration: 650; easing.type: Easing.InOutSine }
                }
            }
            Text {
                width: parent.width
                text: "LunaDash"
                color: Theme.text
                font.family: Theme.font
                font.pixelSize: 28
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                width: parent.width
                text: splash.delayed ? shell.tr("The desktop is taking longer to start. Check the session log.") : shell.tr("Preparing your desktop…")
                color: Theme.muted
                font.family: Theme.font
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
            ShellButton {
                visible: splash.delayed
                anchors.horizontalCenter: parent.horizontalCenter
                text: shell.tr("Dismiss")
                onClicked: { splash.finished = true; splash.reveal = 0 }
            }
        }
    }
    Behavior on reveal { NumberAnimation { duration: Theme.animations ? 280 : 0; easing.type: Easing.OutCubic } }
}
