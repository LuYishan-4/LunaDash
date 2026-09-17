import QtQuick
import QtQuick.Layouts
import Quickshell.Io
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16

    property string section: "devices"
    property var hardwareDevices: []
    property var blockDevices: []
    property string probeError: ""
    property string diskActionMessage: ""

    function refreshHardware() {
        probeError = ""
        if (!deviceProbe.running)
            deviceProbe.running = true
        if (!diskProbe.running)
            diskProbe.running = true
    }

    function parseHardware(text) {
        const devices = []
        let mode = ""
        let current = null
        for (const raw of String(text).split("\n")) {
            const line = raw.trim()
            if (line === "__PCI__") { mode = "pci"; current = null; continue }
            if (line === "__USB__") { mode = "usb"; current = null; continue }
            if (!line) continue
            if (mode === "pci") {
                const match = line.match(/^([0-9a-fA-F:.]+)\s+"([^"]*)"\s+"([^"]*)"\s+"([^"]*)"/)
                if (match) {
                    current = {bus:"PCI", id:match[1], category:match[2], vendor:match[3], name:match[4], driver:"", modules:""}
                    devices.push(current)
                } else if (current && line.startsWith("Kernel driver in use:")) {
                    current.driver = line.substring(line.indexOf(":") + 1).trim()
                } else if (current && line.startsWith("Kernel modules:")) {
                    current.modules = line.substring(line.indexOf(":") + 1).trim()
                }
            } else if (mode === "usb") {
                const match = line.match(/^Bus\s+(\d+)\s+Device\s+(\d+):\s+ID\s+([^\s]+)\s*(.*)$/)
                if (match)
                    devices.push({bus:"USB", id:"Bus " + match[1] + " / Device " + match[2], category:"USB", vendor:match[3], name:match[4] || "USB device", driver:"", modules:""})
            }
        }
        hardwareDevices = devices
    }

    function flattenBlocks(nodes, depth) {
        const output = []
        for (const node of (nodes || [])) {
            const copy = Object.assign({}, node)
            copy.depth = depth
            output.push(copy)
            output.push.apply(output, flattenBlocks(node.children || [], depth + 1))
        }
        return output
    }

    function parseDisks(text) {
        try {
            const object = JSON.parse(text)
            blockDevices = flattenBlocks(object.blockdevices || [], 0)
        } catch (error) {
            probeError = shell.tr("Could not parse disk information.")
            blockDevices = []
        }
    }

    function diskAction(action, path) {
        if (!path || diskAction.running)
            return
        diskActionMessage = ""
        diskAction.command = ["udisksctl", action, "-b", String(path)]
        diskAction.running = true
    }

    PageTitle { shell: page.shell; title: "Device manager and disks" }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        ShellButton { text: shell.tr("Device Manager"); active: page.section === "devices"; onClicked: page.section = "devices" }
        ShellButton { text: shell.tr("Disk Management"); active: page.section === "disks"; onClicked: page.section = "disks" }
        Item { Layout.fillWidth: true }
        ShellButton { text: shell.tr("Refresh"); onClicked: page.refreshHardware() }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "devices"
        title: shell.tr("Device Manager")
        description: shell.tr("Inspect PCI and USB hardware, driver bindings and kernel modules. Scan for hardware changes or open the full hardware and package tools from the same page.")

        RowLayout {
            Layout.fillWidth: true
            ShellButton {
                text: shell.tr("Scan for hardware changes")
                enabled: !rescan.running
                onClicked: rescan.running = true
            }
            ShellButton { text: shell.tr("Hardware details"); onClicked: shell.command("system-tool", "hardware") }
            ShellButton { text: shell.tr("Driver and system updates"); onClicked: shell.command("system-tool", "packages") }
            Item { Layout.fillWidth: true }
        }

        Text {
            Layout.fillWidth: true
            visible: page.hardwareDevices.length === 0
            text: shell.tr("No PCI or USB devices were reported. Install pciutils and usbutils for the complete device list.")
            color: Theme.muted
            font.family: Theme.font
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: page.hardwareDevices
            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: deviceColumn.implicitHeight + 20
                radius: 12
                color: Theme.control
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: deviceColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 3
                    Text {
                        Layout.fillWidth: true
                        text: modelData.name || modelData.category
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: [modelData.bus, modelData.id, modelData.vendor, modelData.category].filter(Boolean).join("  ·  ")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: [modelData.driver ? shell.tr("Driver") + ": " + modelData.driver : "",
                               modelData.modules ? shell.tr("Modules") + ": " + modelData.modules : ""].filter(Boolean).join("  ·  ")
                        color: Theme.accent
                        font.family: Theme.font
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "devices"
        title: shell.tr("Device properties and administration")
        description: shell.tr("Windows Device Manager property areas are represented here by live status, driver/module information, hardware identifiers, rescan controls, and the detailed hardware tool. Device power policy is managed by the Power page; printers and scanners are available below.")
        ToolList { shell: page.shell; category: "devices" }
    }

    SettingsComponents.SettingsCard {
        visible: page.section === "disks"
        title: shell.tr("Disk Management")
        description: shell.tr("Inspect disks and partitions, mount or unmount filesystems, safely power off removable drives, and open privileged partition, format, initialize, resize and SMART operations.")

        Text {
            Layout.fillWidth: true
            visible: page.blockDevices.length === 0
            text: shell.tr("No block devices were reported. Install util-linux for lsblk support.")
            color: Theme.muted
            font.family: Theme.font
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: page.blockDevices
            Rectangle {
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: diskRow.implicitHeight + 20
                radius: 12
                color: Theme.control
                border.width: 1
                border.color: Theme.border

                RowLayout {
                    id: diskRow
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 10
                    spacing: 10

                    ColumnLayout {
                        Layout.leftMargin: modelData.depth * 16
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: [modelData.name, modelData.label, modelData.model].filter(Boolean).join("  ·  ")
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 13
                            font.weight: modelData.type === "disk" ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: [modelData.path, modelData.size, modelData.fstype, modelData.uuid,
                                   modelData.mountpoints ? modelData.mountpoints.filter(Boolean).join(", ") : "",
                                   modelData.tran].filter(Boolean).join("  ·  ")
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 11
                            wrapMode: Text.WrapAnywhere
                        }
                    }

                    ShellButton {
                        visible: modelData.type !== "disk" && (!modelData.mountpoints || modelData.mountpoints.filter(Boolean).length === 0)
                        text: shell.tr("Mount")
                        onClicked: page.diskAction("mount", modelData.path)
                    }
                    ShellButton {
                        visible: modelData.type !== "disk" && modelData.mountpoints && modelData.mountpoints.filter(Boolean).length > 0
                        text: shell.tr("Unmount")
                        onClicked: page.diskAction("unmount", modelData.path)
                    }
                    ShellButton {
                        visible: modelData.type === "disk" && (modelData.rm || modelData.hotplug)
                        text: shell.tr("Safely remove")
                        onClicked: page.diskAction("power-off", modelData.path)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ShellButton { text: shell.tr("Disks and SMART"); onClicked: shell.command("system-tool", "storage") }
            ShellButton { text: shell.tr("Partition, format and initialize"); onClicked: shell.command("system-tool", "partitioner") }
            Item { Layout.fillWidth: true }
        }
        HelpText {
            shell: page.shell
            message: "Creating or deleting partitions, formatting, changing GPT/MBR, resizing filesystems and editing boot flags can destroy data. LunaDash keeps these controls on this page but delegates confirmation and authorization to the installed disk utility."
        }
    }

    Text {
        Layout.fillWidth: true
        visible: page.probeError.length > 0 || page.diskActionMessage.length > 0
        text: page.probeError || page.diskActionMessage
        color: Theme.accent
        font.family: Theme.font
        wrapMode: Text.WordWrap
    }

    Process {
        id: deviceProbe
        command: ["sh", "-c", "printf '__PCI__\\n'; command -v lspci >/dev/null && lspci -mm -k 2>/dev/null; printf '__USB__\\n'; command -v lsusb >/dev/null && lsusb 2>/dev/null"]
        stdout: StdioCollector { onStreamFinished: page.parseHardware(text) }
        stderr: StdioCollector { onStreamFinished: if (text.trim()) page.probeError = text.trim() }
    }

    Process {
        id: diskProbe
        command: ["lsblk", "-J", "-o", "NAME,KNAME,PATH,TYPE,SIZE,FSTYPE,FSVER,LABEL,UUID,MOUNTPOINTS,MODEL,VENDOR,TRAN,RO,RM,HOTPLUG"]
        stdout: StdioCollector { onStreamFinished: page.parseDisks(text) }
        stderr: StdioCollector { onStreamFinished: if (text.trim()) page.probeError = text.trim() }
    }

    Process {
        id: rescan
        command: ["udevadm", "trigger"]
        onExited: (code, status) => page.refreshHardware()
    }

    Process {
        id: diskAction
        property string output: ""
        stdout: StdioCollector { onStreamFinished: diskAction.output = text.trim() }
        stderr: StdioCollector { onStreamFinished: diskAction.output = text.trim() }
        onExited: (code, status) => {
            page.diskActionMessage = diskAction.output
            page.refreshHardware()
        }
    }

    Component.onCompleted: refreshHardware()
}
