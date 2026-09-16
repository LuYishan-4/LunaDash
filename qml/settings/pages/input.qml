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
    readonly property var inputTools: (shell.state.systemTools || []).filter(tool => tool.category === "input")
    readonly property var imeTool: inputTools.find(tool => tool.id === "ime") || ({available:false, package:"fcitx5-configtool"})

    PageTitle { shell: page.shell; title: "Keyboard and pointer" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Keyboard")
        description: shell.tr("Configure the layout and repeat behavior used by the compositor seat.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Keyboard layout"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            StyledComboBox {
                model: ["us", "gb", "de", "fr", "es", "jp", "tw"]
                currentIndex: model.indexOf((shell.state.appearance || {}).keyboardLayout || "us")
                onActivated: shell.setAppearance({keyboardLayout: currentText})
                Accessible.name: shell.tr("Keyboard layout")
            }
        }
        PreferenceSlider { shell: page.shell; preference: "keyRepeatRate"; label: "Key repeat rate"; minimum: 0; maximum: 60; suffix: " / s" }
        PreferenceSlider { shell: page.shell; preference: "keyRepeatDelay"; label: "Key repeat delay"; minimum: 200; maximum: 1500; step: 50; suffix: " ms" }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Input method")
        description: shell.tr("LunaDash uses Fcitx 5 when it is installed. Configure input methods, layouts, addons, hotkeys, candidate behavior, and per-application options with the Fcitx configuration tool.")
        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                Text { text: shell.tr("Fcitx 5"); color: Theme.text; font.family: Theme.font; font.pixelSize: 15 }
                HelpText { shell: page.shell; message: page.imeTool.available ? "Fcitx configuration is available." : "Install fcitx5-configtool to configure Fcitx from LunaDash." }
            }
            ShellButton {
                text: shell.tr("Configure Fcitx")
                active: true
                enabled: page.imeTool.available
                onClicked: shell.command("system-tool", "ime")
                Accessible.name: shell.tr("Configure Fcitx")
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Input method environment"); color: Theme.text; font.family: Theme.font; Layout.fillWidth: true }
            Text { text: "QT_IM_MODULE=fcitx · GTK_IM_MODULE=fcitx · XMODIFIERS=@im=fcitx"; color: Theme.muted; font.family: Theme.font; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.maximumWidth: 430 }
        }
        HelpText { shell: page.shell; message: "Fcitx is started for LunaDash sessions when available. Native Wayland applications should prefer the compositor/text-input path; toolkit variables remain useful for compatibility applications." }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Pointer and input method")
        description: shell.tr("Pointer acceleration, natural scrolling, and touchpad gestures are supplied by the host in nested sessions. Standalone libinput controls are not available yet.")
        PreferenceSlider { shell: page.shell; preference: "cursorSize"; label: "Cursor size after session restart"; minimum: 16; maximum: 64; step: 8; suffix: " px" }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Input test")
        description: shell.tr("Use this field to verify the keyboard layout, preedit, candidate selection, and Fcitx input path.")
        SoftField { Layout.fillWidth: true; placeholderText: shell.tr("Type here to test your keyboard or input method"); Accessible.name: placeholderText }
    }
}
