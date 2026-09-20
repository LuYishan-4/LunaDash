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
    readonly property var installed: (state.installed || []).filter(plugin => plugin.target === selectedTarget)
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
        }));
        editor.text = JSON.stringify(document, null, 2);
        dirty = false;
    }
    function adopt(next) {
        document = next;
        editor.text = JSON.stringify(next, null, 2);
        dirty = true;
    }
    function builtinValue() {
        return Object.assign({}, target.builtinSettings || {}, document.builtins[selectedTarget] || {});
    }
    function setBuiltin(key, value) {
        const next = JSON.parse(JSON.stringify(document));
        next.builtins[selectedTarget] = Object.assign({}, next.builtins[selectedTarget] || {});
        next.builtins[selectedTarget][key] = value;
        adopt(next);
    }
    function pluginValue(plugin) {
        return document.plugins[plugin.id] || {
            enabled: plugin.enabled,
            mode: plugin.mode,
            settings: plugin.settings || {}
        };
    }
    function setPlugin(plugin, key, value) {
        const next = JSON.parse(JSON.stringify(document));
        next.plugins[plugin.id] = JSON.parse(JSON.stringify(pluginValue(plugin)));
        next.plugins[plugin.id][key] = value;
        adopt(next);
    }
    function setPluginSetting(plugin, key, value) {
        const settings = Object.assign({}, pluginValue(plugin).settings);
        settings[key] = value;
        setPlugin(plugin, "settings", settings);
    }
    onStateChanged: if (!dirty && !saving)
        reload()
    Component.onCompleted: {
        state = incomingState;
        reload();
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
                    page.category = page.categories[index];
                    page.selectedTarget = page.categoryTargets[0]?.id || "";
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
            message: "Shell layout and appearance controls remain below. A duration or gap of -1 follows the desktop preference."
        }
    }

    Repeater {
        model: page.installed
        delegate: SettingsCard {
            id: card
            required property var modelData
            readonly property var config: page.pluginValue(modelData)
            title: modelData.name || modelData.id
            description: modelData.description || ""
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    wrapMode: Text.Wrap
                    color: Theme.muted
                    font.family: Theme.font
                    text: card.modelData.type + " · " + card.modelData.version + " · " + card.modelData.status
                }
                SoftSwitch {
                    checked: card.config.enabled
                    onToggled: page.setPlugin(card.modelData, "enabled", checked)
                }
            }
            StyledComboBox {
                Layout.fillWidth: true
                model: [page.shell.tr("Built-in and plugin"), page.shell.tr("Plugin only")]
                currentIndex: card.config.mode === "replace" ? 1 : 0
                enabled: card.modelData.layoutMode !== "stacking"
                onActivated: index => page.setPlugin(card.modelData, "mode", index === 1 ? "replace" : "augment")
            }
            ExtensionOptions {
                Layout.fillWidth: true
                shell: page.shell
                schema: card.modelData.settingsSchema || {}
                values: card.config.settings || {}
                onEdited: (key, value) => page.setPluginSetting(card.modelData, key, value)
            }
            HelpText {
                shell: page.shell
                message: card.modelData.type === "effect" ? "Native plugins run in the compositor. Enable only trusted code. Changes reload after the current hook finishes." : card.modelData.type === "opengl" ? "OpenGL shader plugins require GPU rendering. Software rendering keeps the built-in feature." : ""
            }
            Text {
                visible: Boolean(card.modelData.error)
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                wrapMode: Text.Wrap
                color: Theme.danger
                font.family: Theme.font
                text: card.modelData.error || ""
            }
            ShellButton {
                visible: Boolean(card.modelData.error)
                text: page.shell.tr("Retry plugin")
                onClicked: page.shell.command("extension-error", JSON.stringify({
                    id: card.modelData.id,
                    error: ""
                }))
            }
        }
    }
    HelpText {
        shell: page.shell
        message: page.installed.length ? "" : "No plugins installed for this feature. The built-in feature remains active."
    }
    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            text: page.shell.tr("Save extensions")
            active: true
            enabled: page.dirty && !page.saving
            onClicked: {
                page.saving = true;
                page.shell.command("extension-save", editor.text);
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
                return;
            page.saving = false;
            page.message = result.error || "Extension settings saved.";
            if (!result.error)
                page.reload();
        }
    }
}
