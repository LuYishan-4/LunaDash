import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var state: shell.state.shellModules || ({})
    readonly property var moduleIds: ["panel", "wallpaper", "launcher", "overview", "settings", "setup", "session", "feedback", "compatibility"]
    property int selectedModuleIndex: 0
    property var visualDocument: ({schemaVersion: 1, modules: {}})
    property bool dirty: false
    property bool saving: false
    property bool confirmReset: false
    property int loadedRevision: -1
    property bool advancedOpen: false
    spacing: 16

    readonly property string currentId: moduleIds[Math.max(0, Math.min(selectedModuleIndex, moduleIds.length - 1))]
    readonly property var currentModule: ((visualDocument.modules || {})[currentId] || ({}))
    readonly property var style: currentModule.style || ({})
    readonly property var custom: currentModule.custom || ({enabled: false, entry: ""})
    readonly property var config: currentModule.config || ({})
    readonly property bool recoveryModule: ["settings", "setup", "feedback"].indexOf(currentId) >= 0
    readonly property int minimumWidth: currentId === "settings" ? 800 : 320
    readonly property int minimumHeight: currentId === "panel" ? 24 : (currentId === "settings" || currentId === "setup" ? 480 : 80)
    readonly property int maximumHeight: currentId === "panel" ? 96 : 2160

    function cloneDocument() {
        return JSON.parse(JSON.stringify(visualDocument || {schemaVersion: 1, modules: {}}))
    }

    function adoptDocument(document, markDirty) {
        visualDocument = document
        editor.text = JSON.stringify(document, null, 2)
        dirty = markDirty
    }

    function reloadEditor() {
        const document = JSON.parse(JSON.stringify(state.document || {schemaVersion: 1, modules: {}}))
        visualDocument = document
        editor.text = JSON.stringify(document, null, 2)
        dirty = false
        loadedRevision = state.revision ?? 0
    }

    function setModuleField(field, value) {
        const document = cloneDocument()
        document.modules[currentId][field] = value
        adoptDocument(document, true)
    }

    function setStyle(field, value) {
        const document = cloneDocument()
        document.modules[currentId].style[field] = value
        adoptDocument(document, true)
    }

    function setCustom(field, value) {
        const document = cloneDocument()
        document.modules[currentId].custom[field] = value
        adoptDocument(document, true)
    }

    function setConfig(field, value) {
        const document = cloneDocument()
        if (!document.modules[currentId].config)
            document.modules[currentId].config = {}
        document.modules[currentId].config[field] = value
        adoptDocument(document, true)
    }

    function saveDocument() {
        saving = true
        shell.command("module-save", editor.text)
    }

    onStateChanged: if (!dirty && loadedRevision !== (state.revision ?? 0)) reloadEditor()

    PageTitle {
        shell: page.shell
        title: "Shell modules"
    }

    HelpText {
        shell: page.shell
        message: "Every shell-modules.json field has a visual control below. Advanced JSON remains available for copying, review, and recovery."
    }

    ShellButton {
        text: shell.tr("Plugin settings")
        onClicked: shell.command("open-settings", "plugins")
    }

    Text { Layout.minimumWidth: 0;
        text: page.state.path || ""
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.WrapAnywhere
        Layout.fillWidth: true
    }

    Text { Layout.minimumWidth: 0;
        visible: page.dirty && page.loadedRevision !== page.state.revision
        text: shell.tr("The file changed externally. Reload before saving to avoid replacing newer changes.")
        color: Theme.danger
        font.family: Theme.font
        wrapMode: Text.WordWrap
        Layout.fillWidth: true
    }

    SettingsCard {
        title: shell.tr("Module")
        description: shell.tr("Choose a shell block, then adjust its JSON fields with visual controls.")

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Schema version")
                color: Theme.muted
                font.family: Theme.font
                Layout.fillWidth: true
            }
            Text {
                text: String(page.visualDocument.schemaVersion ?? 1)
                color: Theme.text
                font.family: Theme.font
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Shell block")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            StyledComboBox {
                translationContext: page.shell
                model: page.moduleIds
                currentIndex: page.selectedModuleIndex
                onActivated: index => page.selectedModuleIndex = index
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Enabled")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.currentModule.enabled ?? true
                enabled: !page.recoveryModule
                onToggled: page.setModuleField("enabled", checked)
            }
        }

        HelpText {
            shell: page.shell
            visible: page.recoveryModule
            message: "Settings, setup, and feedback are recovery modules and cannot be disabled."
        }
    }

    SettingsCard {
        title: shell.tr("Size and placement")
        description: shell.tr("Zero width or height means use the built-in size. Position values are available to custom module layouts.")

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Automatic width")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: (page.style.width ?? 0) === 0
                onToggled: page.setStyle("width", checked ? 0 : page.minimumWidth)
            }
        }

        SettingsSlider {
            visible: (page.style.width ?? 0) !== 0
            label: shell.tr("Width")
            value: page.style.width || page.minimumWidth
            minimum: page.minimumWidth
            maximum: 3840
            step: 16
            suffix: " px"
            onMoved: value => page.setStyle("width", Math.round(value))
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Automatic height")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: (page.style.height ?? 0) === 0
                onToggled: page.setStyle("height", checked ? 0 : page.minimumHeight)
            }
        }

        SettingsSlider {
            visible: (page.style.height ?? 0) !== 0
            label: shell.tr("Height")
            value: page.style.height || page.minimumHeight
            minimum: page.minimumHeight
            maximum: page.maximumHeight
            step: page.currentId === "panel" ? 2 : 16
            suffix: " px"
            onMoved: value => page.setStyle("height", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Margin")
            value: page.style.margin ?? 12
            minimum: 0
            maximum: 64
            step: 1
            suffix: " px"
            onMoved: value => page.setStyle("margin", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Corner radius")
            value: page.style.radius ?? 24
            minimum: 0
            maximum: 64
            step: 1
            suffix: " px"
            onMoved: value => page.setStyle("radius", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Font size")
            value: page.style.fontSize ?? 13
            minimum: 10
            maximum: 28
            step: 1
            suffix: " px"
            onMoved: value => page.setStyle("fontSize", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("X position")
            value: page.style.x ?? 0
            minimum: 0
            maximum: 3840
            step: 8
            suffix: " px"
            onMoved: value => page.setStyle("x", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Y position")
            value: page.style.y ?? 0
            minimum: 0
            maximum: 3840
            step: 8
            suffix: " px"
            onMoved: value => page.setStyle("y", Math.round(value))
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Edge")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            StyledComboBox {
                translationContext: page.shell
                model: page.currentId === "panel" ? ["top", "bottom"] : ["top"]
                currentIndex: Math.max(0, model.indexOf(page.style.edge || "top"))
                onActivated: page.setStyle("edge", currentText)
            }
        }
    }

    SettingsCard {
        title: shell.tr("Colors")
        description: shell.tr("Use inherit to follow the desktop palette, or enter #RRGGBB / #RRGGBBAA.")

        Repeater {
            model: ["background", "foreground", "accent"]
            RowLayout {
                required property string modelData
                Layout.fillWidth: true
                Text {
                    text: shell.tr(modelData.charAt(0).toUpperCase() + modelData.slice(1))
                    color: Theme.text
                    font.family: Theme.font
                    Layout.preferredWidth: 110
                }
                SoftField {
                    id: colorField
                    Layout.fillWidth: true
                    text: String(page.style[modelData] ?? "inherit")
                    placeholderText: "inherit / #RRGGBB"
                    onAccepted: page.setStyle(modelData, text.trim())
                }
                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    color: colorField.text === "inherit" ? Theme.accent : colorField.text
                    border.width: 1
                    border.color: Theme.border
                }
                ShellButton {
                    text: shell.tr("Apply")
                    onClicked: page.setStyle(modelData, colorField.text.trim())
                }
                ShellButton {
                    text: shell.tr("Inherit")
                    onClicked: page.setStyle(modelData, "inherit")
                }
            }
        }
    }

    SettingsCard {
        visible: page.currentId === "panel"
        title: shell.tr("Panel appearance")
        description: shell.tr("Hide only the continuous panel background while keeping workspaces, applications, launcher, tray, and clock visible.")

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Show panel background")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.config.backgroundVisible ?? false
                onToggled: page.setConfig("backgroundVisible", checked)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Contrast shells")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.config.contrastShells ?? true
                onToggled: page.setConfig("contrastShells", checked)
            }
        }

        SettingsSlider {
            label: shell.tr("Contrast shell opacity")
            value: page.config.shellOpacity ?? 20
            minimum: 0
            maximum: 60
            step: 1
            suffix: "%"
            onMoved: value => page.setConfig("shellOpacity", Math.round(value))
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Pill workspaces")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.config.workspacePills ?? true
                onToggled: page.setConfig("workspacePills", checked)
            }
        }

        SettingsSlider {
            label: shell.tr("Inactive workspace width")
            value: page.config.workspaceInactiveWidth ?? 18
            minimum: 8
            maximum: 48
            step: 1
            suffix: " px"
            onMoved: value => page.setConfig("workspaceInactiveWidth", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Active workspace width")
            value: page.config.workspaceActiveWidth ?? 38
            minimum: Math.max(18, page.config.workspaceInactiveWidth ?? 18)
            maximum: 72
            step: 1
            suffix: " px"
            onMoved: value => page.setConfig("workspaceActiveWidth", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Workspace pill height")
            value: page.config.workspacePillHeight ?? 10
            minimum: 4
            maximum: 20
            step: 1
            suffix: " px"
            onMoved: value => page.setConfig("workspacePillHeight", Math.round(value))
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Workspace teleport transition")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.config.workspaceTransition ?? true
                onToggled: page.setConfig("workspaceTransition", checked)
            }
        }

        SettingsSlider {
            label: shell.tr("Transition duration")
            value: page.config.transitionDuration ?? 420
            minimum: 180
            maximum: 900
            step: 20
            suffix: " ms"
            onMoved: value => page.setConfig("transitionDuration", Math.round(value))
        }
    }

    SettingsCard {
        visible: page.currentId === "launcher"
        title: shell.tr("Launcher moon button")
        description: shell.tr("These values are stored in launcher.config and control the moon button in the panel.")

        SettingsSlider {
            label: shell.tr("Button size")
            value: page.config.buttonSize ?? 38
            minimum: 28
            maximum: 64
            step: 1
            suffix: " px"
            onMoved: value => page.setConfig("buttonSize", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Logo scale")
            value: page.config.logoScale ?? 88
            minimum: 50
            maximum: 120
            step: 1
            suffix: "%"
            onMoved: value => page.setConfig("logoScale", Math.round(value))
        }

        SettingsSlider {
            label: shell.tr("Background opacity")
            value: page.config.backgroundOpacity ?? 18
            minimum: 0
            maximum: 100
            step: 1
            suffix: "%"
            onMoved: value => page.setConfig("backgroundOpacity", Math.round(value))
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Accent glow")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.config.glow ?? true
                onToggled: page.setConfig("glow", checked)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Orbit ring")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.config.orbit ?? true
                onToggled: page.setConfig("orbit", checked)
            }
        }
    }

    SettingsCard {
        title: shell.tr("Custom QML")
        description: shell.tr("Custom module code is optional. The entry path is relative to the LunaDash modules directory.")

        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                text: shell.tr("Use custom QML")
                color: Theme.text
                font.family: Theme.font
                Layout.fillWidth: true
            }
            SoftSwitch {
                checked: page.custom.enabled ?? false
                onToggled: page.setCustom("enabled", checked)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: shell.tr("Entry file")
                color: Theme.text
                font.family: Theme.font
                Layout.preferredWidth: 110
            }
            SoftField {
                Layout.fillWidth: true
                text: String(page.custom.entry || "")
                placeholderText: "panel/Main.qml"
                onAccepted: page.setCustom("entry", text.trim())
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            text: shell.tr("Validate configuration")
            onClicked: shell.command("module-validate", editor.text)
        }
        ShellButton {
            text: shell.tr("Save module settings")
            active: true
            enabled: page.dirty && !page.saving
            onClicked: page.saveDocument()
        }
        ShellButton {
            text: shell.tr("Reload from disk")
            onClicked: page.reloadEditor()
        }
        Item {
            Layout.fillWidth: true
        }
        ShellButton {
            text: page.advancedOpen ? shell.tr("Hide advanced JSON") : shell.tr("Show advanced JSON")
            onClicked: page.advancedOpen = !page.advancedOpen
        }
    }

    SettingsCard {
        visible: page.advancedOpen
        title: shell.tr("Advanced JSON")
        description: shell.tr("Direct editing is available for review and recovery. Visual controls above cover every supported JSON field.")

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            clip: true
            SoftTextArea {
                id: editor
                objectName: "moduleJsonEditor"
                font.family: Theme.font
                font.pixelSize: 12
                color: Theme.text
                wrapMode: TextEdit.NoWrap
                selectByMouse: true
                readOnly: page.saving
                onTextChanged: if (activeFocus) page.dirty = true
            }
        }

        RowLayout {
            ShellButton {
                text: shell.tr("Validate JSON")
                onClicked: shell.command("module-validate", editor.text)
            }
            ShellButton {
                text: shell.tr("Save JSON")
                active: true
                enabled: page.dirty && !page.saving
                onClicked: page.saveDocument()
            }
        }
    }

    HelpText {
        shell: page.shell
        message: page.state.status || ""
    }

    HelpText {
        shell: page.shell
        message: JSON.stringify(page.state.errors || {}) === "{}" ? "" : JSON.stringify(page.state.errors)
    }

    ShellButton {
        text: page.confirmReset ? shell.tr("Confirm restore built-in modules") : shell.tr("Restore built-in modules...")
        onClicked: {
            if (page.confirmReset) {
                shell.command("module-reset", "")
                page.dirty = false
                page.loadedRevision = -1
                page.confirmReset = false
            } else {
                page.confirmReset = true
            }
        }
    }

    Connections {
        target: page.shell
        function onCommandCompleted(method, result) {
            if (method !== "module-save" || !page.saving)
                return
            page.saving = false
            if (result.error)
                return
            page.visualDocument = JSON.parse(JSON.stringify(result.shellModules.document))
            editor.text = JSON.stringify(result.shellModules.document, null, 2)
            page.dirty = false
            page.loadedRevision = result.shellModules.revision
        }
    }

    Component.onCompleted: reloadEditor()
}
