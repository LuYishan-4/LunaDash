import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import Quickshell.Widgets
import "../components"
import "../style"

ModuleSurface {
    id: launcher
    moduleId: "launcher"
    anchors { top: true }
    margins.top: Theme.barHeight + moduleMargin
    readonly property real expandedHeight: moduleHeight(screen ? Math.min(650, screen.height - Theme.barHeight - 36) : 650)
    property real animatedHeight: opened ? expandedHeight : 1
    implicitWidth: moduleWidth(540)
    implicitHeight: animatedHeight
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    WlrLayershell.namespace: "lunadah-launcher"
    color: "transparent"
    contentItem.transformOrigin: Item.Top
    contentItem.scale: 0.92 + 0.08 * reveal
    Behavior on animatedHeight { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }

    property var builtins: [
        { builtin: true, id: "settings", name: "Settings", description: "Configure the desktop and system", genericName: "Preferences", keywords: "appearance network power applications", icon: "preferences-system" },
        { builtin: true, id: "files", name: "Files", description: "Browse files and folders", genericName: "File manager", keywords: "home documents downloads", icon: "system-file-manager" },
        { builtin: true, id: "terminal", name: "Terminal", description: "Run commands in a terminal", genericName: "Terminal emulator", keywords: "console shell command", icon: "utilities-terminal" },
        { builtin: true, id: "monitor", name: "System monitor", description: "View system resources and performance", genericName: "Monitor", keywords: "cpu memory performance processes", icon: "utilities-system-monitor" }
    ]

    function value(entry, key) {
        const candidate = entry[key]
        if (candidate === undefined || candidate === null)
            return ""
        return Array.isArray(candidate) ? candidate.join(" ") : String(candidate)
    }
    function installedEntries() {
        return DesktopEntries.applications.values
            .filter(entry => !entry.noDisplay)
            .map(entry => ({
                builtin: false,
                id: value(entry, "id"),
                name: value(entry, "name"),
                description: value(entry, "comment") || value(entry, "description") || value(entry, "genericName"),
                genericName: value(entry, "genericName"),
                keywords: value(entry, "keywords"),
                icon: value(entry, "icon"),
                desktopEntry: entry
            }))
    }
    function iconSource(entry) {
        const identity = String((entry.id || "") + " " + (entry.name || "") + " " + (entry.genericName || "")).toLowerCase()
        let icon = String(entry.icon || "")
        if (!icon && (identity.includes("file") || identity.includes("nautilus") || identity.includes("dolphin"))) icon = "system-file-manager"
        else if (!icon && identity.includes("setting")) icon = "preferences-system"
        else if (!icon && (identity.includes("terminal") || identity.includes("console"))) icon = "utilities-terminal"
        else if (!icon && identity.includes("monitor")) icon = "utilities-system-monitor"
        return Quickshell.iconPath(icon || "application-x-executable")
    }
    function rank(entry, tokens) {
        const fields = [entry.name, entry.description, entry.genericName, entry.id, entry.keywords]
            .map(field => String(field || "").toLocaleLowerCase())
        let score = 0
        for (const token of tokens) {
            let tokenScore = 0
            for (const field of fields) {
                const words = field.split(/[^\p{L}\p{N}]+/u).filter(Boolean)
                if (words.some(word => word.startsWith(token))) tokenScore = Math.max(tokenScore, 3)
                else if (field.includes(token)) tokenScore = Math.max(tokenScore, 1)
            }
            if (!tokenScore) return -1
            score += tokenScore
        }
        return score
    }
    function rankedEntries(query) {
        const tokens = query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean)
        return builtins.concat(installedEntries())
            .map((entry, order) => ({ entry: entry, score: rank(entry, tokens), order: order }))
            .filter(candidate => candidate.score >= 0)
            .sort((left, right) => right.score - left.score || left.order - right.order || left.entry.name.localeCompare(right.entry.name))
            .map(candidate => candidate.entry)
    }
    readonly property var results: rankedEntries(search.text)
    function activate(entry) {
        if (entry.builtin)
            shell.launch(entry.id)
        else {
            entry.desktopEntry.execute()
            shell.launcherOpen = false
        }
    }

    Rectangle { anchors.fill: parent; radius: moduleRadius; color: moduleBackground; border.color: Theme.border }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12
        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: shell.tr("Search applications...")
            color: moduleForeground
            placeholderTextColor: Theme.muted
            font.family: Theme.font
            background: Rectangle { color: Theme.surface; border.color: search.activeFocus ? Theme.accent : Theme.border; radius: 12 }
            focus: true
            Keys.onDownPressed: event => { if (applications.count) { applications.currentIndex = Math.min(applications.currentIndex + 1, applications.count - 1); applications.forceActiveFocus(); event.accepted = true } }
            Keys.onUpPressed: event => { if (applications.count) { applications.currentIndex = applications.count - 1; applications.forceActiveFocus(); event.accepted = true } }
            Keys.onReturnPressed: if (applications.count) launcher.activate(launcher.results[applications.currentIndex < 0 ? 0 : applications.currentIndex])
            Keys.onEnterPressed: if (applications.count) launcher.activate(launcher.results[applications.currentIndex < 0 ? 0 : applications.currentIndex])
            Keys.onEscapePressed: shell.launcherOpen = false
        }
        ListView {
            id: applications
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: launcher.results
            currentIndex: count > 0 ? 0 : -1
            keyNavigationWraps: true
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: applicationRow
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 64
                hoverEnabled: true
                highlighted: ListView.isCurrentItem
                Accessible.name: modelData.name
                background: Rectangle {
                    radius: 12
                    color: applicationRow.highlighted || applicationRow.hovered ? Qt.rgba(launcher.moduleAccent.r, launcher.moduleAccent.g, launcher.moduleAccent.b, 0.14) : "transparent"
                }
                contentItem: Row {
                    spacing: 14
                    ApplicationIcon {
                        shell: launcher.shell
                        width: 36
                        height: 36
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: String(applicationRow.modelData.icon || "")
                        appId: String(applicationRow.modelData.id || "")
                        title: String(applicationRow.modelData.name || "")
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 50
                        spacing: 3
                        Text { width: parent.width; text: shell.tr(applicationRow.modelData.name); color: Theme.text; font.family: Theme.font; font.pixelSize: 14; elide: Text.ElideRight }
                        Text { width: parent.width; text: shell.tr(applicationRow.modelData.description || applicationRow.modelData.genericName); color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideRight }
                    }
                }
                onClicked: launcher.activate(modelData)
                Keys.onReturnPressed: launcher.activate(modelData)
                Keys.onEnterPressed: launcher.activate(modelData)
                Keys.onEscapePressed: shell.launcherOpen = false
            }
            Keys.onUpPressed: event => {
                if (currentIndex <= 0) { search.forceActiveFocus(); event.accepted = true }
                else { currentIndex--; event.accepted = true }
            }
            Keys.onDownPressed: event => { if (count) { currentIndex = (currentIndex + 1) % count; event.accepted = true } }
            Keys.onEscapePressed: shell.launcherOpen = false
        }
        Text {
            visible: applications.count === 0
            Layout.alignment: Qt.AlignHCenter
            text: shell.tr("No applications found")
            color: Theme.muted
            font.family: Theme.font
        }
    }
    onOpenedChanged: if (opened) { search.clear(); search.forceActiveFocus() }
}
