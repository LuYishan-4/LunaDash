import QtQuick
import QtQuick.Layouts
import "../components"
import "../settings/components"
import "../style"

ColumnLayout {
    id: root
    required property var shell
    spacing: 12

    readonly property var extensions: shell.state.extensions || ({})
    readonly property var installedPlugins: extensions.installed || []
    readonly property var targets: extensions.targets || []
    readonly property var settingsTargets: ((shell.state.settingsApi || {}).targets || [])
    readonly property var typeIds: ["all", "effect", "quickshell", "opengl"]
    readonly property var typeLabels: ["All types", "Native effect", "Quickshell", "OpenGL"]
    property string selectedType: "all"

    function pluginsForTarget(targetId) {
        return installedPlugins.filter(plugin =>
            plugin.target === targetId &&
            (selectedType === "all" || plugin.type === selectedType))
    }

    function targetVisible(target) {
        if (selectedType === "all")
            return true
        if ((target.types || []).indexOf(selectedType) >= 0)
            return true
        return installedPlugins.some(plugin =>
            plugin.target === target.id && plugin.type === selectedType)
    }

    readonly property var visibleTargets: targets.filter(target => targetVisible(target))

    function settingsTarget(id) {
        return settingsTargets.find(target => target.id === id) || ({})
    }

    function nativeSettingsId(target) {
        if (target.id === "window-layout") {
            const layout = settingsTargets.find(item => item.type === "layout")
            return layout ? layout.id : ""
        }
        const id = "builtin:" + target.id
        return settingsTarget(id).id ? id : ""
    }

    function pluginSettingsId(plugin) {
        const id = "plugin:" + plugin.id
        return settingsTarget(id).id ? id : ""
    }

    function entriesForTarget(target) {
        const entries = [{
            kind: "native",
            id: "native:" + target.id,
            name: "Native",
            type: "builtin",
            status: "available",
            error: "",
            settingsId: nativeSettingsId(target)
        }]
        pluginsForTarget(target.id).forEach(plugin => entries.push({
            kind: "plugin",
            id: plugin.id,
            name: plugin.name || plugin.id,
            type: plugin.type || "plugin",
            status: plugin.status || (plugin.enabled ? "available" : "disabled"),
            error: plugin.error || "",
            settingsId: pluginSettingsId(plugin)
        }))
        return entries
    }

    function hasSettings(settingsId) {
        if (!settingsId)
            return false
        return Object.keys(settingsTarget(settingsId).schema || {}).length > 0
    }

    SettingsCard {
        Layout.fillWidth: true
        title: root.shell.tr("Plugin settings")
        description: root.shell.tr("Filter by plugin type, expand a target, then choose the native implementation or a plugin to edit its parameters.")

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                text: root.shell.tr("Type")
                color: Theme.muted
                font.family: Theme.font
                font.pixelSize: 12
            }

            StyledComboBox {
                id: typeFilter
                objectName: "plugin-settings-type-filter"
                Layout.fillWidth: true
                translationContext: root.shell
                model: root.typeLabels.map(label => root.shell.tr(label))
                currentIndex: Math.max(0, root.typeIds.indexOf(root.selectedType))
                onActivated: index => root.selectedType = root.typeIds[index]
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8

            Repeater {
                model: root.visibleTargets

                delegate: Rectangle {
                    id: targetRow
                    required property var modelData
                    property bool expanded: false
                    readonly property var plugins: root.pluginsForTarget(modelData.id)
                    objectName: "plugin-target-" + modelData.id

                    Layout.fillWidth: true
                    implicitHeight: targetBody.implicitHeight + 24
                    radius: 14
                    color: Qt.rgba(Theme.surfaceElevated.r, Theme.surfaceElevated.g,
                                   Theme.surfaceElevated.b, 0.62)
                    border.width: 1
                    border.color: Qt.rgba(Theme.starlight.r, Theme.starlight.g,
                                          Theme.starlight.b, 0.18)

                    ColumnLayout {
                        id: targetBody
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: root.shell.tr(targetRow.modelData.name || targetRow.modelData.id)
                                    color: Theme.text
                                    font.family: Theme.font
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: targetRow.modelData.id + " · " +
                                          (targetRow.modelData.types || []).join(" / ")
                                    color: Theme.muted
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle {
                                radius: height / 2
                                implicitWidth: targetCount.implicitWidth + 14
                                implicitHeight: targetCount.implicitHeight + 6
                                color: Qt.rgba(Theme.accent.r, Theme.accent.g,
                                               Theme.accent.b, 0.14)
                                Text {
                                    id: targetCount
                                    objectName: "plugin-target-count-" + targetRow.modelData.id
                                    anchors.centerIn: parent
                                    text: targetRow.plugins.length + " " +
                                          root.shell.tr(targetRow.plugins.length === 1 ? "plugin" : "plugins")
                                    color: Theme.accent
                                    font.family: Theme.font
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }
                            }

                            ShellButton {
                                text: targetRow.expanded ? "−" : "+"
                                Accessible.name: root.shell.tr(targetRow.expanded ? "Collapse target" : "Expand target")
                                onClicked: targetRow.expanded = !targetRow.expanded
                            }
                        }

                        ColumnLayout {
                            visible: targetRow.expanded
                            Layout.fillWidth: true
                            spacing: 7

                            Repeater {
                                model: root.entriesForTarget(targetRow.modelData)

                                delegate: Rectangle {
                                    id: implementationRow
                                    required property var modelData
                                    property bool expanded: false
                                    readonly property bool configurable:
                                        root.hasSettings(modelData.settingsId)

                                    objectName: "plugin-implementation-" + modelData.id
                                    Layout.fillWidth: true
                                    implicitHeight: implementationBody.implicitHeight + 20
                                    radius: 12
                                    color: Qt.rgba(Theme.surface.r, Theme.surface.g,
                                                   Theme.surface.b, 0.72)
                                    border.width: 1
                                    border.color: Qt.rgba(Theme.border.r, Theme.border.g,
                                                          Theme.border.b, 0.72)

                                    ColumnLayout {
                                        id: implementationBody
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 8

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 0
                                                spacing: 2
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: implementationRow.modelData.kind === "native"
                                                          ? root.shell.tr("Native")
                                                          : implementationRow.modelData.name
                                                    color: Theme.text
                                                    font.family: Theme.font
                                                    font.pixelSize: 13
                                                    font.weight: Font.DemiBold
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: implementationRow.modelData.kind === "native"
                                                          ? root.shell.tr("Built-in implementation")
                                                          : implementationRow.modelData.type + " · " +
                                                            implementationRow.modelData.status
                                                    color: Theme.muted
                                                    font.family: Theme.font
                                                    font.pixelSize: 10
                                                    elide: Text.ElideRight
                                                }
                                            }

                                            ShellButton {
                                                text: implementationRow.expanded ? "−" : "+"
                                                Accessible.name: root.shell.tr(
                                                    implementationRow.expanded ? "Collapse settings" : "Expand settings")
                                                onClicked: implementationRow.expanded = !implementationRow.expanded
                                            }
                                        }

                                        ColumnLayout {
                                            visible: implementationRow.expanded
                                            Layout.fillWidth: true
                                            spacing: 8

                                            SettingsTargetEditor {
                                                visible: implementationRow.configurable
                                                Layout.fillWidth: true
                                                shell: root.shell
                                                targetId: implementationRow.modelData.settingsId
                                            }

                                            HelpText {
                                                visible: !implementationRow.configurable
                                                shell: root.shell
                                                message: implementationRow.modelData.error
                                                    ? implementationRow.modelData.error
                                                    : "This implementation has no adjustable parameters."
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            HelpText {
                visible: root.visibleTargets.length === 0
                shell: root.shell
                message: "No targets support the selected plugin type."
            }
        }
    }
}
