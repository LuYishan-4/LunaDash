import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../components" as SettingsComponents
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    spacing: 16
    readonly property var inputTools: (shell.state.settingsTools || shell.state.systemTools || []).filter(tool => tool.category === "input")
    readonly property var imeTool: inputTools.find(tool => tool.id === "ime") || ({available:false, configurable:false, package:"fcitx5 + fcitx5-configtool"})
    readonly property var keyboardLayouts: [
        {value:"us", label:"US"},
        {value:"gb", label:"United Kingdom"},
        {value:"de", label:"German"},
        {value:"fr", label:"French"},
        {value:"es", label:"Spanish"},
        {value:"jp", label:"Japanese"},
        {value:"tw", label:"Taiwan (US physical + Fcitx)"}
    ]
    readonly property string selectedLayout: (shell.state.appearance || {}).keyboardLayout || "us"

    PageTitle { shell: page.shell; title: "Keyboard and pointer" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Keyboard")
        description: shell.tr("Configure the physical keyboard layout used by the compositor seat.")
        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Keyboard layout"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            StyledComboBox {
                model: page.keyboardLayouts.map(entry => entry.label)
                currentIndex: Math.max(0, page.keyboardLayouts.findIndex(entry => entry.value === page.selectedLayout))
                onActivated: shell.setAppearance({keyboardLayout: page.keyboardLayouts[index].value})
                Accessible.name: shell.tr("Keyboard layout")
            }
        }
        HelpText {
            shell: page.shell
            message: page.selectedLayout === "tw"
                ? "Taiwan mode keeps the standard US physical key positions and uses Fcitx for Traditional Chinese input such as Zhuyin or Chewing. The XKB tw symbol map is not used as an input method."
                : "Key repeat uses a fixed desktop-friendly timing of 25 repeats per second after a 600 ms delay."
        }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Input method")
        description: shell.tr("LunaDash uses Fcitx 5 when it is installed. Configure input methods, layouts, addons, hotkeys, candidate behavior, and per-application options with the Fcitx configuration tool.")
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: shell.tr("Fcitx 5"); color: Theme.text; font.family: Theme.font; font.pixelSize: 15 }
                HelpText {
                    shell: page.shell
                    message: page.imeTool.available
                        ? (page.imeTool.configurable ? "Fcitx 5 is running-capable and its configuration tool is available." : "Fcitx 5 is installed, but fcitx5-configtool is not installed.")
                        : "Fcitx 5 was not found in PATH. Install fcitx5 and fcitx5-configtool."
                }
            }
            Rectangle {
                width: 10; height: 10; radius: 5
                color: page.imeTool.available ? Theme.accent : Theme.danger
            }
            ShellButton {
                text: shell.tr("Configure Fcitx")
                active: true
                enabled: page.imeTool.configurable ?? false
                onClicked: shell.command("system-tool", "ime")
                Accessible.name: shell.tr("Configure Fcitx")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { wrapMode: Text.Wrap; Layout.minimumWidth: 0; text: shell.tr("Input method environment"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            Text { text: "QT_IM_MODULE=fcitx · GTK_IM_MODULE=fcitx · XMODIFIERS=@im=fcitx"; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.maximumWidth: 430 }
        }
        HelpText { shell: page.shell; message: "Fcitx is started for LunaDash sessions when available. Traditional Chinese input should be configured inside Fcitx; the compositor keyboard layout only describes physical key positions." }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Pointer and input method")
        description: shell.tr("Pointer acceleration, natural scrolling, and touchpad gestures are supplied by the host in nested sessions. Standalone libinput controls are not available yet.")
        PreferenceSlider { shell: page.shell; preference: "cursorSize"; label: "Cursor size after session restart"; minimum: 16; maximum: 64; step: 8; suffix: " px" }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Input test")
        description: shell.tr("Use this field to verify single key presses, key repeat, the physical layout, Fcitx preedit, and candidate selection.")
        SoftField { Layout.fillWidth: true; placeholderText: shell.tr("Type here to test your keyboard or input method"); Accessible.name: placeholderText }
    }
}
