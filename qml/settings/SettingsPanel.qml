import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
import "components" as SettingsComponents
import "../imagepicker"

ModuleSurface {
    id: settings
    moduleId: "settings"

    property string category: "general"
    property bool maximized: false
    property real expansion: maximized ? 1 : 0
    readonly property bool compact: width < 860
    Behavior on expansion {
        NumberAnimation { duration: Theme.motion; easing.type: Easing.InOutCubic }
    }
    property var categories: [
        {id:"general",name:"General"},
        {id:"appearance",name:"Appearance"},
        {id:"windows",name:"Windows and workspaces"},
        {id:"shortcuts",name:"Keyboard shortcuts"},
        {id:"plugins",name:"Plugins"},
        {id:"modules",name:"Shell modules"},
        {id:"dashboard",name:"Dashboard"},
        {id:"display",name:"Display"},
        {id:"input",name:"Keyboard and pointer"},
        {id:"input-method",name:"Input method"},
        {id:"sound",name:"Sound"},
        {id:"network",name:"Internet and network"},
        {id:"bluetooth",name:"Bluetooth"},
        {id:"devices",name:"Device manager and disks"},
        {id:"power",name:"Power and battery"},
        {id:"privacy",name:"Privacy and accessibility"},
        {id:"system",name:"Users, date and time"},
        {id:"applications",name:"Applications and startup"},
        {id:"about",name:"About LunaDash"}
    ]

    SettingsCatalog { id: catalog }

    readonly property var searchResults: catalog.matches(search.text, shell.tr)
        .filter(result => settings.categories.some(category => category.id === result.page))
    readonly property var currentCategory:
        settings.categories.find(entry => entry.id === settings.category)
        || settings.categories[0]
    readonly property int overlayMargin: Math.max(8, Math.min(moduleMargin, 24))
    readonly property bool updateAuthorizing:
        (shell.updateInstall || {}).state === "running"
        && (shell.updateInstall || {}).stage === "authorization"
    readonly property int configuredX:
        Number.isFinite(Number(moduleStyle.x)) ? Number(moduleStyle.x) : 0
    readonly property int configuredY:
        Number.isFinite(Number(moduleStyle.y)) ? Number(moduleStyle.y) : 0
    readonly property int availableScreenWidth: screen ? screen.width : 1440
    readonly property int availableScreenHeight: screen ? screen.height : 900

    function showCategory(id) {
        if (!settings.categories.some(entry => entry.id === id))
            return
        category = id
        search.clear()
    }
    function openResult(entry) { showCategory(entry.page) }

    anchors.top: true
    anchors.left: true
    readonly property int normalX: Math.max(Theme.panelLeftInset + overlayMargin,
        Math.min(configuredX || Theme.panelLeftInset + overlayMargin, availableScreenWidth - 360))
    readonly property int normalY: Math.max(Theme.panelTopInset + overlayMargin,
        Math.min(configuredY || Theme.panelTopInset + overlayMargin,
                 availableScreenHeight - 280))
    margins.left: Math.round(normalX +
        (Theme.panelLeftInset + overlayMargin - normalX) * expansion)
    margins.top: Math.round(normalY +
        (Theme.panelTopInset + overlayMargin - normalY) * expansion)
    readonly property int maximumWidth: Math.max(1,
        availableScreenWidth - margins.left - overlayMargin - Theme.panelRightInset)
    readonly property int maximumHeight: Math.max(1,
        availableScreenHeight - margins.top - overlayMargin - Theme.panelBottomInset)
    implicitWidth: Math.round(Math.min(maximumWidth,
        moduleWidth(1160) + (maximumWidth - moduleWidth(1160)) * expansion))
    implicitHeight: Math.round(Math.min(maximumHeight,
        moduleHeight(740) + (maximumHeight - moduleHeight(740)) * expansion))
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: updateAuthorizing ? WlrLayer.Bottom : WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-settings"
    WlrLayershell.keyboardFocus: opened && !updateAuthorizing
        ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"

    GlassSurface {
        anchors.fill: parent
        shell: settings.shell
        radius: 28
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        enabled: !settings.shell.pickerOpen
        spacing: 12

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 62
            radius: 20
            color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.28)
            border.width: 1
            border.color: Theme.hairline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 10
                spacing: 12

                ColumnLayout {
                    Layout.preferredWidth: settings.compact ? 100 : 210
                    Layout.minimumWidth: settings.compact ? 90 : 160
                    spacing: 0
                    Text {
                        text: shell.tr("Settings")
                        color: Theme.text
                        font.family: Theme.font
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: shell.tr(settings.currentCategory.name)
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 9
                        elide: Text.ElideRight
                    }
                }

                SoftField {
                    id: search
                    Layout.fillWidth: true
                    Layout.maximumWidth: 540
                    implicitHeight: 40
                    leftPadding: 36
                    placeholderText: shell.tr("Search settings")
                    Accessible.name: placeholderText
                    Shortcut {
                        sequences: [StandardKey.Find]
                        context: Qt.WindowShortcut
                        enabled: settings.opened && !settings.shell.pickerOpen
                        onActivated: {
                            search.forceActiveFocus(Qt.ShortcutFocusReason)
                            search.selectAll()
                            search.prepareInputMethod()
                        }
                    }
                    LineIcon {
                        name: "search"
                        width: 17
                        height: 17
                        anchors.left: parent.left
                        anchors.leftMargin: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Keys.onEscapePressed: {
                        if (text.length)
                            clear()
                        else
                            shell.settingsOpen = false
                    }
                }

                Item { Layout.fillWidth: true }

                Rectangle {
                    visible: !settings.compact
                    implicitWidth: categoryBadge.implicitWidth + 40
                    implicitHeight: 30
                    radius: 15
                    color: Qt.rgba(moduleAccent.r, moduleAccent.g,
                                   moduleAccent.b, 0.12)
                    border.width: 1
                    border.color: Qt.rgba(moduleAccent.r, moduleAccent.g,
                                          moduleAccent.b, 0.34)
                    Row {
                        anchors.centerIn: parent
                        spacing: 6
                        LineIcon {
                            name: settings.currentCategory.id
                            width: 14
                            height: 14
                            ink: moduleAccent
                        }
                        Text {
                            id: categoryBadge
                            text: shell.tr(settings.currentCategory.name)
                            color: Theme.text
                            font.family: Theme.font
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                        }
                    }
                }

                Text {
                    visible: !settings.compact
                    text: (shell.state.update || {}).currentVersion || "1.0.1a"
                    color: Theme.muted
                    font.family: Theme.font
                    font.pixelSize: 9
                }

                ShellButton {
                    text: settings.maximized ? "◱" : "□"
                    quiet: true
                    toolTip: shell.tr(settings.maximized
                        ? "Restore settings size" : "Maximize settings")
                    onClicked: settings.maximized = !settings.maximized
                }

                ShellButton {
                    text: "×"
                    quiet: true
                    toolTip: shell.tr("Quick hide settings")
                    onClicked: shell.settingsOpen = false
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: settings.compact ? 72 : 254 + 28 * settings.expansion
                Layout.minimumWidth: settings.compact ? 60 : 180
                Layout.maximumWidth: 300
                Layout.fillHeight: true
                radius: 22
                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.28)
                border.width: 1
                border.color: Theme.hairline

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 9

                    Text {
                        Layout.fillWidth: true
                        visible: search.text.trim().length > 0
                        text: resultList.count + " "
                            + shell.tr(resultList.count === 1 ? "result" : "results")
                        color: Theme.muted
                        font.family: Theme.font
                        font.pixelSize: 9
                    }

                    ListView {
                        id: resultList
                        visible: search.text.trim().length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: settings.searchResults
                        currentIndex: count > 0 ? 0 : -1
                        ScrollBar.vertical: ScrollBar {}
                        delegate: SearchResultDelegate {
                            required property var modelData
                            entry: modelData
                            shell: settings.shell
                            highlighted: ListView.isCurrentItem
                            onClicked: settings.openResult(entry)
                        }
                    }

                    Text {
                        visible: resultList.visible && resultList.count === 0
                        Layout.fillWidth: true
                        text: shell.tr("No settings found")
                        color: Theme.muted
                        font.family: Theme.font
                        wrapMode: Text.WordWrap
                    }

                    ListView {
                        id: categoryList
                        visible: !resultList.visible
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: settings.categories
                        ScrollBar.vertical: ScrollBar {}

                        delegate: Rectangle {
                            id: categoryRow
                            required property var modelData
                            readonly property bool selected:
                                settings.category === modelData.id
                            width: ListView.view.width
                            height: settings.compact ? 42 : Math.max(42, categoryLabel.implicitHeight + 18)
                            radius: 12
                            color: selected
                                ? Qt.rgba(settings.moduleAccent.r,
                                          settings.moduleAccent.g,
                                          settings.moduleAccent.b, 0.16)
                                : categoryMouse.containsMouse
                                    ? Theme.surfaceElevated : "transparent"
                            border.width: selected ? 1 : 0
                            border.color: Qt.rgba(settings.moduleAccent.r,
                                                  settings.moduleAccent.g,
                                                  settings.moduleAccent.b, 0.34)

                            Row {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.margins: 12
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 10
                                LineIcon {
                                    name: modelData.id
                                    width: 18
                                    height: 18
                                    ink: categoryRow.selected
                                        ? settings.moduleAccent : Theme.muted
                                }
                                Text {
                                    id: categoryLabel
                                    visible: !settings.compact
                                    width: Math.max(0, parent.width - 28)
                                    text: shell.tr(modelData.name)
                                    color: categoryRow.selected
                                        ? Theme.text : Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                }
                            }
                            MouseArea {
                                id: categoryMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settings.showCategory(modelData.id)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 0
                radius: 24
                color: Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.28)
                border.width: 1
                border.color: Theme.hairline
                clip: true

                SettingsPageView {
                    anchors.fill: parent
                    anchors.margins: settings.compact ? 12 : 22
                    shell: settings.shell
                    category: settings.category
                }
            }
        }
    }

    ImagePicker {
        anchors.fill: parent
        cornerRadius: Theme.radiusHero
        shell: settings.shell
        opened: settings.shell.pickerOpen
        onClosed: {
            settings.shell.pickerOpen = false
            Qt.callLater(function() {
                if (settings.opened && !settings.shell.pickerOpen) {
                    search.forceActiveFocus(Qt.OtherFocusReason)
                    search.prepareInputMethod()
                }
            })
        }
    }

    onOpenedChanged: {
        if (opened) {
            Qt.callLater(function() {
                if (settings.opened && !settings.shell.pickerOpen) {
                    search.forceActiveFocus(Qt.OtherFocusReason)
                    search.prepareInputMethod()
                }
            })
        } else {
            search.focus = false
            resultList.focus = false
            categoryList.focus = false
        }
    }

    Component.onCompleted: showCategory(shell.settingsPage || "general")
}
