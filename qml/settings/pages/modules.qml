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
    readonly property var targets: (shell.state.settingsApi || {}).targets || []
    readonly property var types: ["All types"].concat([...new Set(targets.map(item => item.type))].sort())
    readonly property var categories: ["All categories"].concat([...new Set(targets.map(item => item.category))].sort())
    property string typeFilter: "module"
    property string categoryFilter: "All categories"
    property string searchText: ""
    property bool configurableOnly: true
    property string selectedId: ""
    property bool advancedOpen: false
    property bool confirmReset: false
    readonly property var visibleTargets: targets.filter(item =>
        (typeFilter === "All types" || item.type === typeFilter) &&
        (categoryFilter === "All categories" || item.category === categoryFilter) &&
        (!configurableOnly || Object.keys(item.schema || {}).length > 0) &&
        [item.id, item.name, item.type, item.category].join(" ").toLowerCase().includes(searchText.toLowerCase()))
    readonly property var current: visibleTargets.find(item => item.id === selectedId) || visibleTargets[0] || ({})
    readonly property string moduleId: String(current.id || "").startsWith("module:") ? current.id.split(":")[1] : ""
    readonly property var moduleDescriptor: (state.descriptors || []).find(item => item.id === moduleId) || ({})
    spacing: 16

    PageTitle { shell: page.shell; title: "Modular settings" }
    HelpText {
        shell: page.shell
        message: "Filter settings by implementation type or category. Effects use the same controls as shell modules; their options do not require custom QML."
    }
    SettingsCard {
        title: page.shell.tr("Filter settings")
        RowLayout {
            Layout.fillWidth: true
            StyledComboBox {
                Layout.fillWidth: true
                translationContext: page.shell
                model: page.types
                currentIndex: page.types.indexOf(page.typeFilter)
                onActivated: index => { page.typeFilter = page.types[index]; page.selectedId = "" }
            }
            StyledComboBox {
                Layout.fillWidth: true
                translationContext: page.shell
                model: page.categories
                currentIndex: page.categories.indexOf(page.categoryFilter)
                onActivated: index => { page.categoryFilter = page.categories[index]; page.selectedId = "" }
            }
        }
        SoftField {
            Layout.fillWidth: true
            placeholderText: page.shell.tr("Search settings")
            onTextChanged: page.searchText = text.trim()
        }
        SoftSwitch {
            text: page.shell.tr("Only configurable targets")
            checked: page.configurableOnly
            onToggled: page.configurableOnly = checked
        }
        StyledComboBox {
            Layout.fillWidth: true
            enabled: page.visibleTargets.length > 0
            model: page.visibleTargets.map(item => page.shell.tr(item.name) + " · " + item.id)
            currentIndex: page.visibleTargets.findIndex(item => item.id === page.current.id)
            onActivated: index => page.selectedId = page.visibleTargets[index].id
        }
        HelpText {
            visible: page.visibleTargets.length === 0
            shell: page.shell
            message: "No settings match these filters."
        }
    }
    SettingsCard {
        visible: Boolean(page.current.id)
        title: page.shell.tr(page.current.name || "Settings")
        description: (page.current.type || "") + " · " + (page.current.category || "")
        SettingsTargetEditor {
            Layout.fillWidth: true
            shell: page.shell
            targetId: page.current.id || ""
        }
    }
    SettingsCard {
        visible: page.moduleId.length > 0
        title: page.shell.tr("Custom module code")
        HelpText {
            shell: page.shell
            message: "Custom QML runs with your user permissions. Allow only trusted code. Disabling custom code keeps the built-in modules available."
        }
        SoftSwitch {
            text: page.shell.tr("Allow custom QML")
            checked: Boolean(page.state.trusted)
            onToggled: page.shell.command("module-code-trust", checked ? "true" : "false")
        }
        ShellButton {
            visible: Boolean(page.moduleDescriptor.template)
            text: page.shell.tr("Install module template")
            onClicked: page.shell.command("module-template", page.moduleId)
        }
        HelpText { shell: page.shell; message: page.state.codeRoot || "" }
    }
    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            text: page.shell.tr("Plugin settings")
            onClicked: page.shell.command("open-settings", "plugins")
        }
        ShellButton {
            text: page.shell.tr("Inspect module JSON")
            onClicked: page.advancedOpen = !page.advancedOpen
        }
    }
    SettingsCard {
        visible: page.advancedOpen
        title: page.shell.tr("Module JSON")
        description: page.state.path || ""
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 280
            clip: true
            SoftTextArea {
                objectName: "moduleJsonEditor"
                text: JSON.stringify(page.state.document || {}, null, 2)
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.NoWrap
                font.family: Theme.font
            }
        }
    }
    HelpText { shell: page.shell; message: page.state.status || "" }
    HelpText {
        shell: page.shell
        message: Object.keys(page.state.errors || {}).length ? JSON.stringify(page.state.errors) : ""
    }
    ShellButton {
        text: page.shell.tr(page.confirmReset ? "Confirm restore built-in modules" : "Restore built-in modules...")
        onClicked: {
            if (page.confirmReset) {
                page.shell.command("module-reset", "")
                page.confirmReset = false
            } else page.confirmReset = true
        }
    }
}
