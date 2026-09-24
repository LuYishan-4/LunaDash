import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../modules"
import "../components"
import "../style"

ModuleSurface {
    id: orbit
    moduleId: "orbit"
    implicitWidth: moduleWidth(860)
    implicitHeight: moduleHeight(650)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-orbit"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    readonly property var document: ((shell.state.orbit || {}).document || {})
    readonly property var engines: document.searchEngines || []
    property int engineIndex: Math.max(0, engines.findIndex(entry => entry.id === document.defaultEngine))
    property var folders: []
    onEnginesChanged: if (engineIndex >= engines.length)
        engineIndex = 0
    readonly property var items: folders.length ? folders[folders.length - 1].children : document.items || []
    readonly property bool compact: width < 700 || height < 560

    function back() {
        if (folders.length)
            folders = folders.slice(0, -1);
        else
            shell.orbitOpen = false;
    }
    function cycleEngine(direction) {
        if (engines.length)
            engineIndex = (engineIndex + direction + engines.length) % engines.length;
    }
    function search() {
        if (!query.text.trim().length || !engines.length)
            return;
        shell.openUrl(engines[engineIndex].url.replace("{query}", encodeURIComponent(query.text.trim())));
        shell.orbitOpen = false;
    }
    function activate(entry) {
        if (entry.children) {
            folders = folders.concat([entry]);
            return;
        }
        if (entry.url)
            shell.openUrl(entry.url);
        else if (entry.command)
            shell.command("launch-command", JSON.stringify(entry.command));
        else if (entry.desktopId) {
            const app = DesktopEntries.applications.values.find(application => application.id === entry.desktopId);
            if (!app) {
                shell.notify(shell.tr("Orbit launcher"), shell.tr("Application is not installed"), "error", entry.desktopId);
                return;
            }
            shell.command("launch-application", JSON.stringify({
                desktopId: app.id,
                command: app.command
            }));
        } else if (entry.action === "wallpapers")
            shell.command("choose-wallpaper", "");
        else if (entry.action === "eye-care")
            shell.command("eye-care", "toggle");
        else if (entry.action === "scratchpad")
            shell.command("scratchpad", "");
        else if (entry.action === "power")
            shell.logoutOpen = true;
        else if (entry.action === "clipboard")
            shell.clipboardPopupOpen = true;
        else if (entry.action === "dashboard")
            shell.setAppearance({
                overview: !shell.overviewOpen
            });
        else if (entry.action)
            shell.launch(entry.action);
        shell.orbitOpen = false;
    }

    onOpenedChanged: if (opened)
        Qt.callLater(() => query.forceActiveFocus())
    Component.onCompleted: if (opened)
        Qt.callLater(() => query.forceActiveFocus())

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLarge
        color: Theme.surfaceStrong
        border.width: 1
        border.color: Theme.hairline
        clip: true

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                ShellButton {
                    text: orbit.folders.length ? shell.tr("Back") : shell.tr("Applications")
                    iconName: orbit.folders.length ? "chevronRight" : "apps"
                    onClicked: {
                        if (orbit.folders.length)
                            orbit.back();
                        else {
                            shell.orbitOpen = false;
                            shell.openLauncherFromMouse();
                        }
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: orbit.folders.length ? shell.tr(orbit.folders[orbit.folders.length - 1].name) : shell.tr("Orbit launcher")
                    color: Theme.muted
                    font.family: Theme.font
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
                ShellButton {
                    iconName: "close"
                    Accessible.name: shell.tr("Close")
                    onClicked: shell.orbitOpen = false
                }
            }
            Item {
                id: constellation
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                Rectangle {
                    visible: !orbit.compact
                    anchors.centerIn: parent
                    width: Math.min(parent.width * 0.80, parent.height * 0.95)
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.hairline
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width * 0.77
                        height: width
                        radius: width / 2
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.hairline
                    }
                }
                ColumnLayout {
                    id: searchBox
                    width: orbit.compact ? parent.width : Math.min(300, parent.width * 0.4)
                    x: (parent.width - width) / 2
                    y: orbit.compact ? 0 : (parent.height - height) / 2
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr("Search or ask…")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: orbit.compact ? 20 : 28
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                    }
                    SoftField {
                        id: query
                        Layout.fillWidth: true
                        placeholderText: orbit.engines.length ? orbit.engines[orbit.engineIndex].name : shell.tr("Search")
                        onAccepted: orbit.search()
                        Keys.onTabPressed: event => {
                            orbit.cycleEngine(1);
                            event.accepted = true;
                        }
                        Keys.onBacktabPressed: event => {
                            orbit.cycleEngine(-1);
                            event.accepted = true;
                        }
                        Keys.onEscapePressed: orbit.back()
                        Keys.onPressed: event => {
                            if (event.modifiers === Qt.AltModifier && event.key >= Qt.Key_1 && event.key <= Qt.Key_8) {
                                const index = event.key - Qt.Key_1;
                                if (index < orbit.items.length)
                                    orbit.activate(orbit.items[index]);
                                event.accepted = true;
                            }
                        }
                    }
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        ShellButton {
                            iconName: "chevronRight"
                            rotation: 180
                            Accessible.name: shell.tr("Previous search engine")
                            onClicked: orbit.cycleEngine(-1)
                        }
                        Text {
                            Layout.maximumWidth: 170
                            text: orbit.engines.length ? orbit.engines[orbit.engineIndex].name : ""
                            color: Theme.accent
                            font.family: Theme.font
                            elide: Text.ElideRight
                        }
                        ShellButton {
                            iconName: "chevronRight"
                            Accessible.name: shell.tr("Next search engine")
                            onClicked: orbit.cycleEngine(1)
                        }
                    }
                }
                Repeater {
                    model: orbit.items
                    delegate: Rectangle {
                        id: capsule
                        required property var modelData
                        required property int index
                        readonly property real angle: -Math.PI / 2 + index * Math.PI * 2 / Math.max(1, orbit.items.length)
                        width: orbit.compact ? (constellation.width - 12) / 2 : 174
                        height: orbit.compact ? Math.max(40, Math.min(68, (constellation.height - searchBox.height - 36) / Math.ceil(orbit.items.length / 2))) : 74
                        x: orbit.compact ? (index % 2) * (width + 12) : constellation.width / 2 + Math.cos(angle) * (constellation.width / 2 - width / 2 - 4) - width / 2
                        y: orbit.compact ? searchBox.height + 18 + Math.floor(index / 2) * (height + 8) : constellation.height / 2 + Math.sin(angle) * (constellation.height / 2 - height / 2 - 4) - height / 2
                        radius: height / 2
                        color: mouse.containsMouse || activeFocus ? Theme.accent : Theme.surfaceElevated
                        border.width: 1
                        border.color: mouse.containsMouse || activeFocus ? Theme.accent : Theme.hairline
                        activeFocusOnTab: true
                        Accessible.role: Accessible.Button
                        Accessible.name: shell.tr(modelData.name)
                        Keys.onReturnPressed: orbit.activate(modelData)
                        Keys.onSpacePressed: orbit.activate(modelData)
                        Keys.onEscapePressed: orbit.back()
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 10
                            LineIcon {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                name: capsule.modelData.icon || (capsule.modelData.children ? "files" : "search")
                                ink: mouse.containsMouse || capsule.activeFocus ? Theme.accentInk : Theme.accent
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3
                                Text {
                                    Layout.fillWidth: true
                                    text: shell.tr(capsule.modelData.name)
                                    color: mouse.containsMouse || capsule.activeFocus ? Theme.accentInk : Theme.text
                                    font.family: Theme.font
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    visible: capsule.height >= 64
                                    text: capsule.modelData.description ? shell.tr(capsule.modelData.description) : shell.tr("Alt+%1").arg(capsule.index + 1)
                                    color: mouse.containsMouse || capsule.activeFocus ? Theme.accentInk : Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }
                        }
                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: orbit.activate(capsule.modelData)
                        }
                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.motionFast
                            }
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                text: shell.tr("Tab: search engine · Alt+1–8: open item · Esc: back")
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }
}
