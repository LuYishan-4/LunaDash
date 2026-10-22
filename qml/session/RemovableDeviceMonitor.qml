import QtQuick
import Quickshell.Io

Item {
    id: monitor
    required property var shell

    property bool storageInitialized: false
    property bool usbInitialized: false
    property var knownUsbDisks: []
    property var knownUsbDevices: []
    property var latestDevices: []
    property double lastUsbNotificationMs: 0

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

    function announceUsb(name, details) {
        lastUsbNotificationMs = Date.now()
        shell.notify(shell.tr("USB device connected"), shell.tr("Connected") + ": " + name,
                     "usb", details || name)
        if (!soundProcess.running)
            soundProcess.running = true
    }

    function adoptUsb(text) {
        const devices = []
        for (const raw of String(text).split("\n")) {
            const line = raw.trim()
            if (!line)
                continue
            const match = line.match(/^Bus\s+(\d+)\s+Device\s+(\d+):\s+ID\s+([^\s]+)\s*(.*)$/)
            if (!match)
                continue
            devices.push({
                key: match[1] + ":" + match[2] + ":" + match[3],
                id: match[3],
                name: match[4] || match[3],
                bus: match[1],
                device: match[2]
            })
        }

        const keys = devices.map(device => device.key)
        if (usbInitialized) {
            for (const device of devices) {
                if (knownUsbDevices.indexOf(device.key) < 0)
                    announceUsb(device.name, "USB ID " + device.id + "\nBus " + device.bus + " · Device " + device.device)
            }
        }
        usbInitialized = true
        knownUsbDevices = keys
    }

    function adoptStorage(text) {
        try {
            const object = JSON.parse(text)
            const devices = flatten(object.blockdevices || [], false, "")
            const diskKeys = devices.filter(item => item.type === "disk").map(item => item.path)
            if (storageInitialized) {
                for (const path of diskKeys) {
                    if (knownUsbDisks.indexOf(path) < 0 && Date.now() - lastUsbNotificationMs > 1800) {
                        const disk = devices.find(item => item.path === path) || ({})
                        const name = disk.label || disk.model || path
                        announceUsb(name, [disk.model, disk.size, path].filter(Boolean).join("\n"))
                    }
                }
            }
            storageInitialized = true
            knownUsbDisks = diskKeys
            latestDevices = devices
            shell.removableDevices = devices
        } catch (error) {
            console.warn("Could not parse removable-device status: " + error)
        }
    }

    Process {
        id: storageProbe
        command: ["lsblk", "-J", "-o", "PATH,PKNAME,TYPE,LABEL,SIZE,FSTYPE,MOUNTPOINTS,TRAN,RM,HOTPLUG,MODEL"]
        stdout: StdioCollector { onStreamFinished: monitor.adoptStorage(text) }
    }

    Process {
        id: usbProbe
        command: ["lsusb"]
        stdout: StdioCollector { onStreamFinished: monitor.adoptUsb(text) }
    }

    Process {
        id: soundProcess
        command: ["pw-play", "/usr/share/sounds/freedesktop/stereo/device-added.oga"]
    }

    property bool probePending: false
    property bool udevMonitorHealthy: true

    function refreshDevices() {
        if (storageProbe.running || usbProbe.running) {
            probePending = true
            return
        }
        probePending = false
        storageProbe.running = true
        usbProbe.running = true
    }

    Process {
        id: udevMonitor
        command: ["udevadm", "monitor", "--udev", "--property"]
        running: monitor.udevMonitorHealthy
        stdout: SplitParser {
            onRead: data => {
                if (String(data).trim().length)
                    deviceDebounce.restart()
            }
        }
        onExited: {
            // Minimal containers may not have a running udev daemon. Keep a
            // low-frequency fallback instead of spawning lsblk+lsusb every
            // 1.2 seconds forever.
            monitor.udevMonitorHealthy = false
            fallbackTimer.start()
        }
    }

    Timer {
        id: deviceDebounce
        interval: 280
        repeat: false
        onTriggered: monitor.refreshDevices()
    }

    Timer {
        id: fallbackTimer
        interval: 15000
        repeat: true
        running: !monitor.udevMonitorHealthy
        onTriggered: monitor.refreshDevices()
    }

    Connections {
        target: storageProbe
        function onRunningChanged() {
            if (!storageProbe.running && !usbProbe.running && monitor.probePending)
                deviceDebounce.restart()
        }
    }
    Connections {
        target: usbProbe
        function onRunningChanged() {
            if (!storageProbe.running && !usbProbe.running && monitor.probePending)
                deviceDebounce.restart()
        }
    }

    Component.onCompleted: {
        monitor.refreshDevices()
    }
}
