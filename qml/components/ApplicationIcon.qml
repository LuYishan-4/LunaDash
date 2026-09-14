import QtQuick
import Quickshell
import "../style"

Item {
    id: root
    required property var shell
    property string iconName: ""
    property string appId: ""
    property string title: ""

    readonly property string identity: (iconName + " " + appId + " " + title).toLowerCase()
    readonly property bool builtinApp: appId === "lunadah-app"
                                       || appId === "lunadah-image-picker"
                                       || iconName === "lunadah"
    readonly property bool internalAlias: ["preferences-system", "applications-system", "preferences-desktop-theme", "preferences-desktop-emoticons", "system-file-manager", "utilities-terminal", "utilities-system-monitor", "hwinfo", "input-keyboard"].includes(iconName)

    function themed(name) {
        if (!name || name === "lunadah")
            return ""
        const path = String(Quickshell.iconPath(name) || "")
        return path.length > 0 && !path.includes("qs-blackhole") ? path : ""
    }

    function desktopIcon() {
        const candidates = [appId, title].filter(value => String(value || "").trim().length > 0)
        for (const candidate of candidates) {
            const entry = DesktopEntries.heuristicLookup(String(candidate).trim())
            if (entry && entry.icon) {
                const path = themed(entry.icon)
                if (path.length > 0)
                    return path
            }
        }
        return ""
    }

    readonly property string fileSource: {
        const candidate = String(iconName || "")
        if (/^(image:|file:|qrc:|data:)/.test(candidate) && !candidate.includes("qs-blackhole")) return candidate
        if (candidate.startsWith("/") && /\.(png|jpe?g|webp|svg|xpm)$/i.test(candidate)) return "file://" + candidate
        return ""
    }
    readonly property string themeSource: {
        if (fileSource.length > 0 || builtinApp || internalAlias) return ""
        const fromDesktop = desktopIcon()
        if (fromDesktop.length > 0) return fromDesktop
        return themed(iconName)
    }
    readonly property string iconSource: fileSource.length > 0 ? fileSource : themeSource
    readonly property string vectorName: {
        if (iconSource.length > 0) return ""
        if (identity.includes("file") || identity.includes("nautilus") || identity.includes("dolphin")) return "files"
        if (identity.includes("setting") || identity.includes("control-center") || identity.includes("preference")) return "settings"
        if (identity.includes("kitty") || identity.includes("terminal") || identity.includes("console")) return "terminal"
        if (identity.includes("monitor") || identity.includes("hwinfo")) return "monitor"
        return "apps"
    }

    Image {
        anchors.fill: parent
        source: root.iconSource
        visible: root.iconSource.length > 0
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        mipmap: true
        smooth: true
    }
    LineIcon {
        anchors.fill: parent
        name: root.vectorName
        ink: Theme.text
        visible: root.iconSource.length === 0
    }
}
