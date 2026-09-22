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
    // Never retain keyboard ownership while the launcher is collapsed. Keeping
    // Exclusive set on a one-pixel hidden surface makes it compete with normal
    // applications for keyboard focus.
    WlrLayershell.keyboardFocus: opened && shell.launcherKeyboardActive
        ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    WlrLayershell.namespace: "lunadash-launcher"
    color: "transparent"
    contentItem.transformOrigin: Item.Top
    contentItem.scale: 0.92 + 0.08 * reveal
    Behavior on animatedHeight { NumberAnimation { duration: Theme.motion; easing.type: Easing.OutCubic } }

    property var builtins: [
        { builtin: true, id: "settings", name: "Settings", description: "Configure the desktop and system", genericName: "Preferences", keywords: "appearance network power applications", icon: "preferences-system" },
        { builtin: true, id: "files", name: "Files", description: "Browse files and folders", genericName: "File manager", keywords: "home documents downloads", icon: "system-file-manager" }
    ]
    function value(entry, key) { const candidate = entry[key]; if (candidate === undefined || candidate === null) return ""; return Array.isArray(candidate) ? candidate.join(" ") : String(candidate) }
    function installedEntries() { return DesktopEntries.applications.values.filter(entry => !entry.noDisplay).map(entry => ({ builtin:false,id:value(entry,"id"),name:value(entry,"name"),description:value(entry,"comment")||value(entry,"description")||value(entry,"genericName"),genericName:value(entry,"genericName"),keywords:value(entry,"keywords"),icon:value(entry,"icon"),desktopEntry:entry })) }
    function rank(entry, tokens) { const fields=[entry.name,entry.description,entry.genericName,entry.id,entry.keywords].map(field=>String(field||"").toLocaleLowerCase()); let score=0; for(const token of tokens){let tokenScore=0;for(const field of fields){const words=field.split(/[^\p{L}\p{N}]+/u).filter(Boolean);if(words.some(word=>word.startsWith(token)))tokenScore=Math.max(tokenScore,3);else if(field.includes(token))tokenScore=Math.max(tokenScore,1)}if(!tokenScore)return -1;score+=tokenScore}return score }
    function rankedEntries(query) { const tokens=query.toLocaleLowerCase().trim().split(/\s+/).filter(Boolean);return builtins.concat(installedEntries()).map((entry,order)=>({entry:entry,score:rank(entry,tokens),order:order})).filter(candidate=>candidate.score>=0).sort((left,right)=>right.score-left.score||left.order-right.order||left.entry.name.localeCompare(right.entry.name)).map(candidate=>candidate.entry) }
    readonly property var results: rankedEntries(search.text)
    function activate(entry) {
        if (entry.builtin) {
            shell.launch(entry.id)
        } else {
            shell.command("launch-application", JSON.stringify({
                desktopId: entry.id,
                command: entry.desktopEntry.command
            }))
        }
        shell.launcherOpen = false
    }
    function focusSearch(selectEverything = true) {
        if (!opened)
            return
        shell.launcherKeyboardActive = true
        Qt.callLater(function() {
            if (!launcher.opened || !shell.launcherKeyboardActive)
                return
            search.forceActiveFocus(Qt.PopupFocusReason)
            search.prepareInputMethod()
            if (selectEverything)
                search.selectAll()
        })
    }

    Rectangle { anchors.fill: parent; radius: moduleRadius; color: moduleBackground; border.color: Theme.border }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 12
        SoftField {
            id: search
            Layout.fillWidth: true
            placeholderText: shell.tr("Search applications...")
            focus: false
            activeFocusOnTab: true
            TapHandler { onTapped: launcher.focusSearch() }
            Keys.onDownPressed: event => { if (applications.count) { applications.currentIndex = Math.min(applications.currentIndex + 1, applications.count - 1); applications.forceActiveFocus(); event.accepted = true } }
            Keys.onUpPressed: event => { if (applications.count) { applications.currentIndex = applications.count - 1; applications.forceActiveFocus(); event.accepted = true } }
            Keys.onReturnPressed: if (applications.count) launcher.activate(launcher.results[applications.currentIndex < 0 ? 0 : applications.currentIndex])
            Keys.onEnterPressed: if (applications.count) launcher.activate(launcher.results[applications.currentIndex < 0 ? 0 : applications.currentIndex])
            Keys.onEscapePressed: shell.launcherOpen = false
        }
        RowLayout {
            visible: shell.launcherKeyboardActive
            Layout.fillWidth: true
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: applications.count + " " + shell.tr(applications.count === 1 ? "application" : "applications")
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 10
            }
            Text {
                text: shell.tr("↑↓ Navigate  ·  Enter Open  ·  Esc Close")
                color: Qt.rgba(Theme.muted.r, Theme.muted.g, Theme.muted.b, 0.78)
                font.family: Theme.font
                font.pixelSize: 9
            }
        }
        ListView {
            id: applications; Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4; model: launcher.results; currentIndex: count > 0 ? 0 : -1; keyNavigationWraps: true
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: applicationRow
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 66
                hoverEnabled: true
                highlighted: ListView.isCurrentItem
                scale: down ? 0.985 : hovered || highlighted ? 1.006 : 1
                Accessible.name: modelData.name

                background: Rectangle {
                    radius: 13
                    color: applicationRow.highlighted
                        ? Qt.rgba(launcher.moduleAccent.r, launcher.moduleAccent.g, launcher.moduleAccent.b, 0.18)
                        : applicationRow.hovered
                            ? Theme.surfaceElevated
                            : "transparent"
                    border.width: applicationRow.highlighted ? 1 : 0
                    border.color: Qt.rgba(launcher.moduleAccent.r, launcher.moduleAccent.g, launcher.moduleAccent.b, 0.36)
                    Rectangle {
                        visible: applicationRow.highlighted
                        width: 3
                        radius: 1.5
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 10
                        color: launcher.moduleAccent
                    }
                    Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                }

                contentItem: Row {
                    spacing: 14
                    ApplicationIcon {
                        shell: launcher.shell
                        width: 38
                        height: 38
                        anchors.verticalCenter: parent.verticalCenter
                        iconName: String(applicationRow.modelData.icon || "")
                        appId: String(applicationRow.modelData.id || "")
                        title: String(applicationRow.modelData.name || "")
                        scale: applicationRow.hovered || applicationRow.highlighted ? 1.06 : 1
                        Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
                    }
                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, parent.width - 82)
                        spacing: 3
                        Text {
                            width: parent.width
                            text: shell.tr(applicationRow.modelData.name)
                            color: applicationRow.highlighted ? Theme.moon : Theme.text
                            font.family: Theme.font
                            font.pixelSize: 14
                            font.weight: applicationRow.highlighted ? Font.DemiBold : Font.Medium
                            elide: Text.ElideRight
                            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
                        }
                        Text {
                            width: parent.width
                            text: shell.tr(applicationRow.modelData.description || applicationRow.modelData.genericName)
                            color: Theme.muted
                            font.family: Theme.font
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                    }
                    LineIcon {
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        name: "chevronRight"
                        ink: launcher.moduleAccent
                        opacity: applicationRow.hovered || applicationRow.highlighted ? 0.95 : 0.25
                        transform: Translate { x: applicationRow.hovered || applicationRow.highlighted ? 3 : 0 }
                        Behavior on opacity { NumberAnimation { duration: Theme.motionFast } }
                    }
                }

                onClicked: launcher.activate(modelData)
                Keys.onReturnPressed: launcher.activate(modelData)
                Keys.onEnterPressed: launcher.activate(modelData)
                Keys.onEscapePressed: shell.launcherOpen = false
                Behavior on scale { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
            }
            Keys.onUpPressed: event => { if (currentIndex <= 0) { launcher.focusSearch(); event.accepted = true } else { currentIndex--; event.accepted = true } }
            Keys.onDownPressed: event => { if (count) { currentIndex = (currentIndex + 1) % count; event.accepted = true } }
            Keys.onEscapePressed: shell.launcherOpen = false
            Keys.onPressed: event => {
                // Typing while the result list owns focus should immediately
                // return input to the search field instead of leaking the key
                // to another client.
                if (event.text && event.text.length > 0 && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
                    launcher.focusSearch()
                    search.insert(search.cursorPosition, event.text)
                    event.accepted = true
                }
            }
        }
        Text { visible: applications.count === 0; Layout.alignment: Qt.AlignHCenter; text: shell.tr("No applications found"); color: Theme.muted; font.family: Theme.font }
    }
    onOpenedChanged: {
        if (opened) {
            search.clear()
            if (shell.launcherOpenSource === "keyboard")
                Qt.callLater(function() { launcher.focusSearch(false) })
            else {
                search.focus = false
                applications.focus = false
            }
        } else {
            shell.launcherKeyboardActive = false
            search.focus = false
            applications.focus = false
        }
    }
}
