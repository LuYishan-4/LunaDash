import QtQuick
import "../style"

Item {
    id: root
    required property var shell
    property string iconName: ""
    property string appId: ""
    property string title: ""

    readonly property string identity: (iconName + " " + appId + " " + title).toLowerCase()
    readonly property string localSource: {
        const candidate = iconName
        if (candidate === "lunadah") return String(shell.iconSource)
        if (/^(image:|file:|qrc:|data:)/.test(candidate) && !candidate.includes("qs-blackhole")) return candidate
        if (candidate.startsWith("/") && /\.(png|jpe?g|webp|svg|xpm)$/i.test(candidate)) return "file://" + candidate
        return ""
    }
    readonly property string vectorName: {
        if (identity.includes("file") || identity.includes("nautilus") || identity.includes("dolphin")) return "files"
        if (identity.includes("setting") || identity.includes("control-center") || identity.includes("preference")) return "settings"
        if (identity.includes("kitty") || identity.includes("terminal") || identity.includes("console")) return "terminal"
        if (identity.includes("monitor") || identity.includes("hwinfo")) return "monitor"
        return ""
    }
    readonly property string badge: {
        const value = title || appId || iconName || "?"
        return value.trim().charAt(0).toUpperCase() || "?"
    }

    Image {
        anchors.fill: parent
        source: root.localSource
        visible: root.localSource.length > 0
        fillMode: Image.PreserveAspectFit
        asynchronous: true
        mipmap: true
        smooth: true
    }
    LineIcon {
        anchors.fill: parent
        name: root.vectorName
        ink: Theme.text
        visible: root.localSource.length === 0 && root.vectorName.length > 0
    }
    Rectangle {
        anchors.fill: parent
        visible: root.localSource.length === 0 && root.vectorName.length === 0
        radius: Math.min(width, height) * 0.28
        color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22)
        border.width: 1
        border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.52)
        Text {
            anchors.centerIn: parent
            text: root.badge
            color: Theme.text
            font.pixelSize: Math.max(9, Math.min(parent.width, parent.height) * 0.54)
            font.bold: true
        }
    }
}
