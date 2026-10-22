import QtQuick
import QtQuick.Window
import Quickshell
import "ApplicationIdentity.js" as Identity
import "../style"

Item {
    id: root
    required property var shell
    property string iconName: ""
    property string appId: ""
    property string title: ""

    readonly property bool builtinApp: appId === "lunadash-app"
                                       || iconName === "lunadash"
    readonly property bool internalAlias: ["preferences-system", "applications-system", "preferences-desktop-theme", "preferences-desktop-emoticons", "system-file-manager", "utilities-terminal", "utilities-system-monitor", "hwinfo", "input-keyboard"].includes(iconName)

    function themed(name) {
        if (!Identity.isThemeIconName(name))
            return ""
        const path = String(Quickshell.iconPath(name) || "")
        return path.length > 0 && !path.includes("qs-blackhole") ? path : ""
    }

    function desktopIcon() {
        const entry = Identity.desktopEntry(appId, DesktopEntries.applications.values)
        return entry ? (Identity.fileSource(entry.icon) || themed(entry.icon)) : ""
    }

    readonly property string fileSource: Identity.fileSource(iconName)
    readonly property string themeSource: {
        if (fileSource.length > 0 || builtinApp || internalAlias) return ""
        const fromDesktop = desktopIcon()
        if (fromDesktop.length > 0) return fromDesktop
        return themed(iconName)
    }
    readonly property string iconSource: fileSource.length > 0 ? fileSource : themeSource
    readonly property string vectorName: Identity.vectorName(iconName)

    Image {
        id: iconImage
        anchors.fill: parent
        source: root.iconSource
        visible: status === Image.Ready
        // Bound the raster size. Without it an SVG icon is decoded at its
        // intrinsic size, and QtSvg refuses oversized masks, leaving the icon
        // blank or showing undecoded pixels.
        sourceSize.width: Math.max(1, Math.round(width * Screen.devicePixelRatio))
        sourceSize.height: Math.max(1, Math.round(height * Screen.devicePixelRatio))
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        mipmap: true
        smooth: true
    }
    LineIcon {
        anchors.fill: parent
        name: root.vectorName
        ink: Theme.text
        visible: iconImage.status !== Image.Ready
    }
}
