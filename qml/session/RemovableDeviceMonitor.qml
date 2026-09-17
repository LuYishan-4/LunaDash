import QtQuick
import Quickshell.Io

Item {
    id: monitor
    required property var shell
    property bool initialized: false
    property var knownUsbDisks: []
    property var latestDevices: []

    function flatten(nodes, inheritedUsb, parentDisk) {
        const result = []
        for (const node of (nodes || [])) {
            const usb = inheritedUsb || String(node.tran || "").toLowerCase() === "usb"
            const diskPath = node.type === "disk" ? node.path : parentDisk
            if (usb) {
                const points = (node.mountpoints || []).filter(Boolean)
                result.push({
                    path: node.path || "",
                    diskPath: diskPath || node.path || "",
                    type: node.type || "",
                    label: node.label || "",
                    model: node.model || "",
                    size: node.size || "",
                    fstype: node.fstype || "",
                    mounted: points.length > 0,
                    mountpoint: points.length > 0 ? points[0] : "",
                    removable: Boolean(node.rm || node.hotplug),
                    storage: node.type === "disk" || node.type === "part" || String(node.fstype || "").length > 0
                })
            }
            result.push.apply(result, flatten(node.children || [], usb, diskPath))
        }
        return result
    }

    function adopt(text) {
        try {
            const object = JSON.parse(text)
            const devices = flatten(object.blockdevices || [], false, "")
            const diskKeys = devices.filter(item => item.type === "disk").map(item => item.path)
            if (initialized) {
                for (const path of diskKeys) {
                    if (knownUsbDisks.indexOf(path) < 0) {
                        const disk = devices.find(item => item.path === path) || ({})
                        const title = shell.tr("USB device connected")
                        const name = disk.label || disk.model || path
                        shell.notify(title, shell.tr("Connected") + ": " + name, "usb",
                                     [disk.model, disk.size, path].filter(Boolean).join("\n"))
                        soundProcess.running = true
                    }
                }
            }
            initialized = true
            knownUsbDisks = diskKeys
            latestDevices = devices
            shell.removableDevices = devices
        } catch (error) {
            console.warn("Could not parse removable-device status: " + error)
        }
    }

    Process {
        id: probe
        command: ["lsblk", "-J", "-o", "PATH,PKNAME,TYPE,LABEL,SIZE,FSTYPE,MOUNTPOINTS,TRAN,RM,HOTPLUG,MODEL"]
        stdout: StdioCollector { onStreamFinished: monitor.adopt(text) }
    }

    Process {
        id: soundProcess
        command: ["sh", "-c", "if command -v canberra-gtk-play >/dev/null 2>&1; then canberra-gtk-play -i device-added >/dev/null 2>&1; elif command -v pw-play >/dev/null 2>&1 && [ -f /usr/share/sounds/freedesktop/stereo/device-added.oga ]; then pw-play /usr/share/sounds/freedesktop/stereo/device-added.oga >/dev/null 2>&1; elif command -v paplay >/dev/null 2>&1 && [ -f /usr/share/sounds/freedesktop/stereo/device-added.oga ]; then paplay /usr/share/sounds/freedesktop/stereo/device-added.oga >/dev/null 2>&1; fi"]
    }

    Timer {
        interval: 1200
        repeat: true
        running: true
        triggeredOnStart: true
        onTriggered: if (!probe.running) probe.running = true
    }
}
