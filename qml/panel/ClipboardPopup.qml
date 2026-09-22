import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: popup
    moduleId: "clipboard"
    anchors { top: true; right: true }
    margins {
        top: Theme.barHeight + moduleMargin + 8
        right: moduleMargin + 8
    }
    implicitWidth: moduleWidth(420)
    implicitHeight: moduleHeight(Math.min(560, Math.max(176, 112 + history.length * 68)))
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-clipboard"
    color: "transparent"

    property var history: []
    property bool closeArmed: false

    function refresh() {
        if (!listProcess.running) {
            listProcess.command = [shell.clipboardExecutable, "list"]
            listProcess.running = true
        }
    }

    function copy(index) {
        if (copyProcess.running)
            return
        copyProcess.command = [shell.clipboardExecutable, "copy", String(index)]
        copyProcess.running = true
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Qt.rgba(Theme.surfaceOpaque.r, Theme.surfaceOpaque.g, Theme.surfaceOpaque.b, 0.98)
        border.width: 1
        border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.24)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.preferredWidth: 34
                Layout.preferredHeight: 34
                radius: 11
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
                LineIcon { anchors.centerIn: parent; width: 18; height: 18; name: "copy"; ink: Theme.accent }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 1
                Text {
                    text: shell.tr("Clipboard")
                    color: Theme.text
                    font.family: Theme.font
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    text: history.length + " " + shell.tr(history.length === 1 ? "entry" : "entries")
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 10
                }
            }

            ShellButton {
                text: shell.tr("Clear")
                enabled: history.length > 0 && !clearProcess.running
                onClicked: {
                    clearProcess.command = [shell.clipboardExecutable, "clear"]
                    clearProcess.running = true
                }
            }
            ShellButton { text: "×"; onClicked: shell.clipboardPopupOpen = false }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(Theme.starlight.r, Theme.starlight.g, Theme.starlight.b, 0.10)
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: popup.history
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 58
                radius: 12
                color: rowMouse.containsMouse
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.13)
                    : Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.54)
                border.width: 1
                border.color: rowMouse.containsMouse
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.30)
                    : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.55)
                scale: rowMouse.pressed ? 0.992 : rowMouse.containsMouse ? 1.004 : 1

                readonly property string previewText: {
                    const value = String(modelData.text || "").replace(/\s+/g, " ").trim()
                    return value.length > 150 ? value.slice(0, 147) + "…" : value
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        Layout.preferredWidth: 24
                        text: String(row.index + 1)
                        color: Theme.accent
                        font.family: Theme.font
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: row.previewText || shell.tr("Empty text")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                    }
                    LineIcon {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        name: "copy"
                        ink: rowMouse.containsMouse ? Theme.accent : Theme.muted
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: popup.copy(row.index)
                }

                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
                Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            }
        }

        Text {
            visible: popup.history.length === 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            text: shell.tr("Clipboard history is empty")
            color: Theme.muted
            font.family: Theme.font
            font.pixelSize: 12
        }
    }

    HoverHandler {
        id: hover
        onHoveredChanged: {
            if (hovered) {
                popup.closeArmed = true
                closeTimer.stop()
            } else if (popup.closeArmed) {
                closeTimer.restart()
            }
        }
    }
    Timer {
        id: armTimer
        interval: 500
        onTriggered: {
            popup.closeArmed = true
            if (!hover.hovered)
                closeTimer.restart()
        }
    }
    Timer {
        id: closeTimer
        interval: 320
        onTriggered: if (!hover.hovered && popup.opened)
            shell.clipboardPopupOpen = false
    }
    Timer {
        interval: 900
        running: popup.opened
        repeat: true
        triggeredOnStart: true
        onTriggered: popup.refresh()
    }

    Process {
        id: listProcess
        stdout: StdioCollector {
            onStreamFinished: {
                try {
                    const parsed = JSON.parse(text || "[]")
                    popup.history = Array.isArray(parsed) ? parsed : []
                } catch (error) {
                    popup.history = []
                }
            }
        }
    }
    Process {
        id: copyProcess
        stderr: StdioCollector {
            onStreamFinished: if (text.trim())
                shell.notify(shell.tr("Clipboard"), text.trim(), "error", text.trim())
        }
        onExited: (code, status) => {
            if (code === 0) {
                popup.refresh()
                shell.clipboardPopupOpen = false
            }
        }
    }
    Process {
        id: clearProcess
        onExited: (code, status) => {
            if (code === 0)
                popup.refresh()
        }
    }

    onOpenedChanged: {
        if (opened) {
            closeArmed = false
            armTimer.restart()
            refresh()
        } else {
            armTimer.stop()
            closeTimer.stop()
        }
    }
}
