import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import "../components"
import "../style"

ModuleSurface {
    id: popup
    moduleId: "feedback"
    extensionTarget: "devices"
    anchors.top: true
    anchors.right: true
    margins.top: Theme.barHeight + 8
    margins.right: 10
    implicitWidth: moduleWidth(410)
    implicitHeight: moduleHeight(Math.max(150, Math.min(520, deviceColumn.implicitHeight + 46)))
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-usb-devices"
    color: "transparent"

    function run(action, path) {
        if (!path || diskAction.running)
            return
        diskAction.command = ["udisksctl", action, "-b", String(path)]
        diskAction.running = true
    }

    Rectangle {
        anchors.fill: parent
        color: moduleBackground
        radius: moduleRadius
        border.width: 1
        border.color: Qt.rgba(moduleAccent.r, moduleAccent.g, moduleAccent.b, 0.36)
    }

    ColumnLayout {
        id: deviceColumn
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            LineIcon { name: "usb"; width: 20; height: 20; ink: moduleAccent }
            Text { text: shell.tr("USB devices"); color: moduleForeground; font.family: Theme.font; font.pixelSize: 16; font.weight: Font.DemiBold; Layout.fillWidth: true }
            ShellButton { text: "×"; Accessible.name: shell.tr("Close"); onClicked: shell.usbPopupOpen = false }
        }

        Text {
            Layout.fillWidth: true
            visible: (shell.removableDevices || []).length === 0
            text: shell.tr("No USB devices are connected.")
            color: Theme.muted
            font.family: Theme.font
        }

        Repeater {
            model: shell.removableDevices || []
            Rectangle {
                required property var modelData
                visible: modelData.storage
                Layout.fillWidth: true
                implicitHeight: usbRow.implicitHeight + 18
                radius: 12
                color: Theme.control
                border.width: 1
                border.color: Theme.border

                RowLayout {
                    id: usbRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 9
                    spacing: 8

                    LineIcon { name: "usb"; width: 18; height: 18; ink: Theme.accent }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: modelData.label || modelData.model || modelData.path
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: [modelData.path, modelData.size, modelData.fstype, modelData.mountpoint].filter(Boolean).join("  ·  ")
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 10
                            elide: Text.ElideMiddle
                        }
                    }
                    ShellButton {
                        visible: modelData.type !== "disk" && !modelData.mounted
                        text: shell.tr("Mount")
                        onClicked: popup.run("mount", modelData.path)
                    }
                    ShellButton {
                        visible: modelData.type !== "disk" && modelData.mounted
                        text: shell.tr("Open")
                        onClicked: Quickshell.execDetached(["xdg-open", modelData.mountpoint])
                    }
                    ShellButton {
                        visible: modelData.type !== "disk" && modelData.mounted
                        text: shell.tr("Unmount")
                        onClicked: popup.run("unmount", modelData.path)
                    }
                    ShellButton {
                        visible: modelData.type === "disk"
                        text: shell.tr("Eject")
                        onClicked: popup.run("power-off", modelData.path)
                    }
                }
            }
        }
    }

    Process {
        id: diskAction
        stderr: StdioCollector {
            onStreamFinished: if (text.trim()) shell.notify(shell.tr("USB device"), text.trim(), "error", text.trim())
        }
    }
}
