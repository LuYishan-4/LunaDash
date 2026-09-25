import QtQuick
import QtQuick.Layouts
import "../../components"
import "../../style"

SettingsCard {
    id: controls
    required property var shell
    readonly property var targets: (shell.state.settingsApi || {}).targets || []
    readonly property var styleTarget: targets.find(item => item.id === "module:panel:style") || ({})
    readonly property var configTarget: targets.find(item => item.id === "module:panel:config") || ({})
    readonly property var panelStyle: styleTarget.values || ({})
    readonly property var panelConfig: configTarget.values || ({})
    property string pendingTarget: ""
    property string error: ""
    readonly property bool saving: pendingTarget.length > 0
    readonly property bool available: Boolean(styleTarget.revision && configTarget.revision)
    title: shell.tr("Panel layout")
    description: shell.tr("Change the panel position, size and contents. Changes apply immediately.")

    function update(section, field, value) {
        if (saving || !available)
            return
        const target = section === "style" ? styleTarget : configTarget
        if ((target.values || {})[field] === value)
            return
        const changes = {}
        changes[field] = value
        pendingTarget = target.id
        error = ""
        shell.command("settings-update", JSON.stringify({
            target: target.id, revision: target.revision, changes: changes
        }))
    }

    ColumnLayout {
        Layout.fillWidth: true
        enabled: controls.available && !controls.saving
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: controls.shell.tr("Panel edge")
                wrapMode: Text.Wrap
                color: Theme.text
                font.family: Theme.font
            }
            StyledComboBox {
                Layout.preferredWidth: 180
                translationContext: controls.shell
                model: ["Top", "Bottom", "Left", "Right"]
                currentIndex: Math.max(0, ["top", "bottom", "left", "right"].indexOf(controls.panelStyle.edge || "top"))
                Accessible.name: controls.shell.tr("Panel edge")
                onActivated: index => controls.update("style", "edge", ["top", "bottom", "left", "right"][index])
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: controls.shell.tr("Panel length")
                wrapMode: Text.Wrap
                color: Theme.text
                font.family: Theme.font
            }
            StyledComboBox {
                Layout.preferredWidth: 180
                translationContext: controls.shell
                model: ["Full length", "Custom length"]
                currentIndex: controls.panelStyle.width > 0 ? 1 : 0
                Accessible.name: controls.shell.tr("Panel length")
                onActivated: index => controls.update("style", "width", index === 0 ? 0 : 960)
            }
        }
        Repeater {
            model: [
                {key: "width", label: "Panel width", minimum: 320, maximum: 3840},
                {key: "height", label: "Panel height", minimum: 24, maximum: 96},
                {key: "margin", label: "Panel margin", minimum: 0, maximum: 64}
            ]
            RowLayout {
                id: dimensionRow
                required property var modelData
                Layout.fillWidth: true
                visible: modelData.key !== "width" || controls.panelStyle.width > 0
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: controls.shell.tr(dimensionRow.modelData.label)
                    color: Theme.text
                    font.family: Theme.font
                    wrapMode: Text.Wrap
                }
                SoftField {
                    property bool userEdited: false
                    Layout.preferredWidth: 92
                    text: String(dimensionRow.modelData.key === "height"
                        ? controls.panelStyle.height || (controls.shell.state.appearance || {}).panelHeight || 40
                        : controls.panelStyle[dimensionRow.modelData.key] ?? 10)
                    clearButtonEnabled: false
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: dimensionRow.modelData.minimum; top: dimensionRow.modelData.maximum }
                    Accessible.name: controls.shell.tr(dimensionRow.modelData.label)
                    onTextEdited: userEdited = true
                    onEditingFinished: {
                        const commit = userEdited && acceptableInput
                        userEdited = false
                        if (commit)
                            controls.update("style", dimensionRow.modelData.key, Number(text))
                    }
                }
                Text { text: "px"; color: Theme.muted; font.family: Theme.font }
            }
        }
        HelpText {
            shell: controls.shell
            message: "Width controls the length along the selected edge. Height controls panel thickness. Short panels keep essential controls and scroll their workspace and application lists."
        }
        Repeater {
            model: [
                {key: "centerLauncher", label: "Centered launcher", fallback: true},
                {key: "showActiveTitle", label: "Show active window title", fallback: true},
                {key: "showSystemStats", label: "Show CPU and memory", fallback: true},
                {key: "occupiedWorkspacesOnly", label: "Compact workspace list", fallback: true},
                {key: "backgroundVisible", label: "Show background", fallback: false},
                {key: "contrastShells", label: "Contrast shells", fallback: true}
            ]
            RowLayout {
                id: toggleRow
                required property var modelData
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: controls.shell.tr(toggleRow.modelData.label)
                    color: Theme.text
                    font.family: Theme.font
                    wrapMode: Text.Wrap
                }
                SoftSwitch {
                    checked: controls.panelConfig[toggleRow.modelData.key] ?? toggleRow.modelData.fallback
                    Accessible.name: controls.shell.tr(toggleRow.modelData.label)
                    onToggled: controls.update("config", toggleRow.modelData.key, checked)
                }
            }
        }
        HelpText {
            shell: controls.shell
            message: "Show at least two workspaces and one spare after the last occupied workspace. All configured workspaces remain available."
        }
        Repeater {
            model: [
                {key: "shellOpacity", label: "Shell opacity", fallback: 20, maximum: 60},
                {key: "capsuleTint", label: "Capsule tint", fallback: 12, maximum: 100}
            ]
            RowLayout {
                id: colorRow
                required property var modelData
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: controls.shell.tr(colorRow.modelData.label)
                    color: Theme.text
                    font.family: Theme.font
                    wrapMode: Text.Wrap
                }
                SoftField {
                    property bool userEdited: false
                    Layout.preferredWidth: 92
                    text: String(controls.panelConfig[colorRow.modelData.key] ?? colorRow.modelData.fallback)
                    clearButtonEnabled: false
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: IntValidator { bottom: 0; top: colorRow.modelData.maximum }
                    Accessible.name: controls.shell.tr(colorRow.modelData.label)
                    onTextEdited: userEdited = true
                    onEditingFinished: {
                        const commit = userEdited && acceptableInput
                        userEdited = false
                        if (commit)
                            controls.update("config", colorRow.modelData.key, Number(text))
                    }
                }
                Text { text: "0–" + colorRow.modelData.maximum; color: Theme.muted; font.family: Theme.font }
            }
        }
        HelpText {
            shell: controls.shell
            message: "Opacity ranges from transparent (0) to opaque (60). Tint mixes in the secondary color; lower values keep capsules dark."
        }
        Text {
            Layout.fillWidth: true
            text: controls.shell.tr("Launcher image")
            color: Theme.text
            font.family: Theme.font
            font.weight: Font.DemiBold
        }
        Text {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: controls.panelConfig.launcherImage || controls.shell.tr("LunaDash logo")
            textFormat: Text.PlainText
            color: Theme.muted
            font.family: Theme.font
            elide: Text.ElideMiddle
        }
        RowLayout {
            Layout.fillWidth: true
            ShellButton {
                Layout.fillWidth: true
                text: controls.shell.tr("Choose launcher image")
                onClicked: {
                    controls.shell.pickerPurpose = "panel-launcher"
                    controls.shell.pickerOpen = true
                }
            }
            ShellButton {
                Layout.fillWidth: true
                text: controls.shell.tr("Restore LunaDash logo")
                enabled: Boolean(controls.panelConfig.launcherImage)
                onClicked: controls.update("config", "launcherImage", "")
            }
        }
        ShellButton {
            text: controls.shell.tr("More panel options")
            onClicked: controls.shell.command("open-settings", "modules")
        }
    }
    Text {
        visible: controls.error.length > 0
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        text: controls.error
        textFormat: Text.PlainText
        color: Theme.danger
        font.family: Theme.font
        wrapMode: Text.Wrap
    }
    Connections {
        target: controls.shell
        function onCommandCompleted(method, result) {
            if (method !== "settings-update" || !controls.saving || result.settingsTarget !== controls.pendingTarget)
                return
            controls.pendingTarget = ""
            controls.error = result.error ? controls.shell.tr(result.error) : ""
        }
    }
}
