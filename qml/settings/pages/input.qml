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

    PageTitle { shell: page.shell; title: "Keyboard and pointer" }

    SettingsComponents.SettingsCard {
        title: shell.tr("Keyboard")
        description: shell.tr("Configure the layout and repeat behavior used by the compositor seat.")
        RowLayout {
            Layout.fillWidth: true
            Text { text: shell.tr("Keyboard layout"); color: Theme.text; Layout.fillWidth: true }
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
        title: shell.tr("Pointer and input method")
        description: shell.tr("Pointer acceleration, natural scrolling, and touchpad gestures are supplied by the host in nested sessions. Standalone libinput controls are not available yet.")
        PreferenceSlider { shell: page.shell; preference: "cursorSize"; label: "Cursor size after session restart"; minimum: 16; maximum: 64; step: 8; suffix: " px" }
        ToolList { shell: page.shell; category: "input" }
    }

    SettingsComponents.SettingsCard {
        title: shell.tr("Input test")
        description: shell.tr("Use this field to verify the keyboard layout and Fcitx input path.")
        TextField { Layout.fillWidth: true; placeholderText: shell.tr("Type here to test your keyboard or input method"); color: Theme.text; Accessible.name: placeholderText }
    }
}
