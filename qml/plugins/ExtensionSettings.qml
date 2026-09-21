import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../components"
import "../settings/components"
import "../style"

ColumnLayout {
    id: page
    required property var shell

    readonly property var incomingState: shell.state.extensions || ({})
    property var state: ({})
    onIncomingStateChanged: if (JSON.stringify(incomingState) !== JSON.stringify(state))
        state = incomingState

    readonly property var targets: state.targets || []
    readonly property var categories: [...new Set(targets.map(target => target.category))]
    property string category: categories[0] || "Desktop"
    readonly property var categoryTargets: targets.filter(target => target.category === category)
    property string selectedTarget: "panel"
    readonly property var target: targets.find(target => target.id === selectedTarget) || ({})

    readonly property var installedPlugins: state.installed || []
    readonly property var remotePlugins: state.remote || []
    property int pluginTab: 0
    property int filterIndex: 0
    property string searchText: ""
    readonly property var filterOptions: pluginTab === 0
        ? ["All plugins", "Enabled", "Disabled", "Quickshell", "Native", "OpenGL"]
        : ["All plugins", "Installed locally", "Available remotely", "Quickshell", "Native", "OpenGL"]

    property var document: ({
        schemaVersion: 1,
        builtins: {},
        plugins: {}
    })
    property bool dirty: false
    property bool saving: false
    property bool advanced: false
    property string message: ""
    spacing: 16

    function reload() {
        document = JSON.parse(JSON.stringify(state.document || {
            schemaVersion: 1,
            builtins: {},
            plugins: {}
        }))
        editor.text = JSON.stringify(document, null, 2)
        dirty = false
    }

    function adopt(next) {
        document = next
        editor.text = JSON.stringify(next, null, 2)
        dirty = true
    }

    function builtinValue() {
        return Object.assign({}, target.builtinSettings || {}, document.builtins[selectedTarget] || {})
    }

    function setBuiltin(key, value) {
        const next = JSON.parse(JSON.stringify(document))
        next.builtins[selectedTarget] = Object.assign({}, next.builtins[selectedTarget] || {})
        next.builtins[selectedTarget][key] = value
        adopt(next)
    }

    function pluginValue(plugin) {
        return document.plugins[plugin.id] || {
            enabled: plugin.enabled,
            mode: plugin.mode,
            settings: plugin.settings || {}
        }
    }

    function setPlugin(plugin, key, value) {
        const next = JSON.parse(JSON.stringify(document))
        next.plugins[plugin.id] = JSON.parse(JSON.stringify(pluginValue(plugin)))
        next.plugins[plugin.id][key] = value
        adopt(next)
    }

    function setPluginSetting(plugin, key, value) {
        const settings = Object.assign({}, pluginValue(plugin).settings)
        settings[key] = value
        setPlugin(plugin, "settings", settings)
    }

    function matchesSearch(plugin) {
        const query = searchText.trim().toLowerCase()
        if (!query.length)
            return true
        const tags = (plugin.tags || []).join(" ")
        return [plugin.name, plugin.id, plugin.description, plugin.author, plugin.type,
                plugin.target, tags].join(" ").toLowerCase().includes(query)
    }

    function matchesFilter(plugin) {
        const value = filterOptions[filterIndex] || "All plugins"
        if (value === "All plugins")
            return true
        if (value === "Enabled")
            return Boolean(pluginValue(plugin).enabled)
        if (value === "Disabled")
            return !Boolean(pluginValue(plugin).enabled)
        if (value === "Installed locally")
            return Boolean(plugin.installed)
        if (value === "Available remotely")
            return !Boolean(plugin.installed)
        if (value === "Quickshell")
            return plugin.type === "quickshell"
        if (value === "Native")
            return plugin.type === "effect"
        if (value === "OpenGL")
            return plugin.type === "opengl"
        return true
    }

    readonly property var visiblePlugins: (pluginTab === 0 ? installedPlugins : remotePlugins)
        .filter(plugin => matchesSearch(plugin) && matchesFilter(plugin))

    onPluginTabChanged: filterIndex = 0
    onStateChanged: if (!dirty && !saving)
        reload()

    Component.onCompleted: {
        state = incomingState
        reload()
    }

    SettingsCard {
        title: page.shell.tr("Plugins")
        description: page.shell.tr("Search installed plugins and the remote catalogue, then filter by state or plugin type.")

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            ShellButton {
                Layout.fillWidth: true
                text: page.shell.tr("Installed")
                active: page.pluginTab === 0
                onClicked: page.pluginTab = 0
            }
            ShellButton {
                Layout.fillWidth: true
                text: page.shell.tr("Store")
                active: page.pluginTab === 1
                onClicked: page.pluginTab = 1
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            SoftField {
                id: searchField
                Layout.fillWidth: true
                placeholderText: page.shell.tr("Search plugins")
                onTextChanged: page.searchText = text
            }

            StyledComboBox {
                Layout.preferredWidth: 190
                translationContext: page.shell
                model: page.filterOptions
                currentIndex: page.filterIndex
                onActivated: index => page.filterIndex = index
            }
        }

        HelpText {
            visible: page.pluginTab === 1 && Boolean(page.state.storeLoading)
            shell: page.shell
            message: "Refreshing plugin catalogue..."
        }

        Text {
            visible: page.pluginTab === 1 && Boolean(page.state.storeError)
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: page.state.storeError || ""
            wrapMode: Text.Wrap
            color: Theme.warning
            font.family: Theme.font
            font.pixelSize: 12
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 10

            Repeater {
                model: page.visiblePlugins

                delegate: Rectangle {
                    id: pluginCard
                    required property var modelData

                    readonly property bool remote: page.pluginTab === 1
                    readonly property bool remoteIcon: String(modelData.icon || "").startsWith("https://")
                    readonly property var config: remote ? ({}) : page.pluginValue(modelData)
                    property bool expanded: false

                    Layout.fillWidth: true
                    implicitHeight: pluginBody.implicitHeight + 28
                    radius: 16
                    color: Qt.rgba(Theme.surfaceElevated.r, Theme.surfaceElevated.g,
                                   Theme.surfaceElevated.b, 0.72)
                    border.width: 1
                    border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g,
                                          Theme.starlight.b, 0.24)

                    ColumnLayout {
                        id: pluginBody
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 9

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                Layout.preferredWidth: 48
                                Layout.preferredHeight: 48
                                radius: 24
                                clip: true
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)

                                readonly property string iconValue: String(pluginCard.modelData.icon || "")
                                readonly property bool directIcon: iconValue.startsWith("https://")
                                    || iconValue.startsWith("file:")
                                    || iconValue.startsWith("qrc:")
                                    || iconValue.startsWith("data:")

                                Image {
                                    id: pluginIconImage
                                    anchors.fill: parent
                                    anchors.margins: 4
                                    source: parent.directIcon ? parent.iconValue : ""
                                    visible: parent.directIcon && status === Image.Ready
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                    smooth: true
                                }

                                LineIcon {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    visible: !pluginIconImage.visible
                                    name: "apps"
                                    ink: Theme.text
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 3

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 7

                                    Text {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: pluginCard.modelData.name || pluginCard.modelData.id
                                        color: Theme.text
                                        font.family: Theme.font
                                        font.pixelSize: 15
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Rectangle {
                                        implicitWidth: versionText.implicitWidth + 12
                                        implicitHeight: versionText.implicitHeight + 5
                                        radius: implicitHeight / 2
                                        color: Qt.rgba(Theme.lavender.r, Theme.lavender.g,
                                                       Theme.lavender.b, 0.26)
                                        Text {
                                            id: versionText
                                            anchors.centerIn: parent
                                            text: pluginCard.modelData.version || ""
                                            color: Theme.lavender
                                            font.family: Theme.font
                                            font.pixelSize: 10
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    visible: Boolean(pluginCard.modelData.description)
                                    text: pluginCard.modelData.description || ""
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: pluginCard.expanded ? 20 : 2
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    visible: Boolean(pluginCard.modelData.author)
                                    text: pluginCard.modelData.author || ""
                                    color: Qt.rgba(Theme.muted.r, Theme.muted.g, Theme.muted.b, 0.82)
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                visible: pluginCard.remote
                                text: page.shell.tr(pluginCard.modelData.installed
                                                    ? "Installed locally"
                                                    : "Available remotely")
                                color: pluginCard.modelData.installed ? Theme.success : Theme.accent
                                font.family: Theme.font
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }

                            SoftSwitch {
                                visible: !pluginCard.remote
                                checked: Boolean(pluginCard.config.enabled)
                                onToggled: page.setPlugin(pluginCard.modelData, "enabled", checked)
                            }

                            ShellButton {
                                visible: !pluginCard.remote
                                text: pluginCard.expanded ? "−" : "+"
                                onClicked: pluginCard.expanded = !pluginCard.expanded
                            }
                        }

                        Flow {
                            Layout.fillWidth: true
                            Layout.preferredHeight: childrenRect.height
                            spacing: 6

                            Repeater {
                                model: pluginCard.modelData.tags || []
                                delegate: Rectangle {
                                    required property var modelData
                                    implicitWidth: tagLabel.implicitWidth + 14
                                    implicitHeight: tagLabel.implicitHeight + 6
                                    radius: implicitHeight / 2
                                    color: Qt.rgba(Theme.starlight.r, Theme.starlight.g,
                                                   Theme.starlight.b, 0.11)
                                    Text {
                                        id: tagLabel
                                        anchors.centerIn: parent
                                        text: String(modelData)
                                        color: Theme.muted
                                        font.family: Theme.font
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            visible: pluginCard.expanded && !pluginCard.remote
                            Layout.fillWidth: true
                            spacing: 9

                            StyledComboBox {
                                Layout.fillWidth: true
                                model: [page.shell.tr("Built-in and plugin"),
                                        page.shell.tr("Plugin only")]
                                currentIndex: pluginCard.config.mode === "replace" ? 1 : 0
                                enabled: pluginCard.modelData.layoutMode !== "stacking"
                                onActivated: index => page.setPlugin(
                                    pluginCard.modelData, "mode", index === 1 ? "replace" : "augment")
                            }

                            ExtensionOptions {
                                Layout.fillWidth: true
                                shell: page.shell
                                schema: pluginCard.modelData.settingsSchema || {}
                                values: pluginCard.config.settings || {}
                                onEdited: (key, value) => page.setPluginSetting(pluginCard.modelData, key, value)
                            }

                            HelpText {
                                shell: page.shell
                                message: pluginCard.modelData.type === "effect"
                                    ? "Native plugins run in the compositor. Enable only trusted code. Changes reload after the current hook finishes."
                                    : pluginCard.modelData.type === "opengl"
                                        ? "OpenGL shader plugins require GPU rendering. Software rendering keeps the built-in feature."
                                        : ""
                            }

                            Text {
                                visible: Boolean(pluginCard.modelData.error)
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                wrapMode: Text.Wrap
                                color: Theme.danger
                                font.family: Theme.font
                                text: pluginCard.modelData.error || ""
                            }

                            ShellButton {
                                visible: Boolean(pluginCard.modelData.error)
                                text: page.shell.tr("Retry plugin")
                                onClicked: page.shell.command("extension-error", JSON.stringify({
                                    id: pluginCard.modelData.id,
                                    error: ""
                                }))
                            }
                        }

                        ShellButton {
                            visible: pluginCard.remote && Boolean(pluginCard.modelData.sourceUrl)
                            text: page.shell.tr("View source")
                            onClicked: page.shell.openUrl(pluginCard.modelData.sourceUrl)
                        }
                    }
                }
            }

            HelpText {
                visible: page.visiblePlugins.length === 0
                shell: page.shell
                message: "No plugins match your search or filter."
            }
        }
    }

    SettingsCard {
        title: shell.tr("Desktop extensions")
        description: shell.tr("Choose a category to configure built-in features and their plugins. Replacement failures restore the built-in feature.")

        RowLayout {
            Layout.fillWidth: true

            StyledComboBox {
                Layout.fillWidth: true
                model: page.categories.map(name => page.shell.tr(name))
                currentIndex: page.categories.indexOf(page.category)
                onActivated: index => {
                    page.category = page.categories[index]
                    page.selectedTarget = page.categoryTargets[0]?.id || ""
                }
            }

            StyledComboBox {
                Layout.fillWidth: true
                model: page.categoryTargets.map(target => page.shell.tr(target.name))
                currentIndex: page.categoryTargets.findIndex(target => target.id === page.selectedTarget)
                onActivated: index => page.selectedTarget = page.categoryTargets[index].id
            }
        }

        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            wrapMode: Text.Wrap
            font.family: Theme.font
            color: Theme.muted
            text: page.selectedTarget + " · " + (page.target.types || []).join(" / ")
        }

        HelpText {
            shell: page.shell
            message: "Built-in settings"
        }

        ExtensionOptions {
            Layout.fillWidth: true
            shell: page.shell
            schema: page.target.settings || {}
            values: page.builtinValue()
            onEdited: (key, value) => page.setBuiltin(key, value)
        }

        HelpText {
            shell: page.shell
            message: "A duration or gap of -1 follows the desktop preference."
        }
    }

    RowLayout {
        Layout.fillWidth: true

        ShellButton {
            text: page.shell.tr("Save extensions")
            active: true
            enabled: page.dirty && !page.saving
            onClicked: {
                page.saving = true
                page.shell.command("extension-save", editor.text)
            }
        }

        ShellButton {
            text: page.shell.tr("Reload from disk")
            onClicked: page.reload()
        }

        ShellButton {
            text: page.shell.tr("Advanced JSON")
            onClicked: page.advanced = !page.advanced
        }
    }

    SettingsCard {
        visible: page.advanced
        title: page.shell.tr("Extension JSON")
        description: page.state.path || ""

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            clip: true

            SoftTextArea {
                id: editor
                objectName: "extensionJsonEditor"
                wrapMode: TextEdit.NoWrap
                font.family: Theme.font
                selectByMouse: true
                readOnly: page.saving
                onTextChanged: if (activeFocus)
                    page.dirty = true
            }
        }
    }

    HelpText {
        shell: page.shell
        message: page.message || page.state.error || ""
    }

    Connections {
        target: page.shell

        function onCommandCompleted(method, result) {
            if (method !== "extension-save" || !page.saving)
                return
            page.saving = false
            page.message = result.error || "Extension settings saved."
            if (!result.error)
                page.reload()
        }
    }
}
