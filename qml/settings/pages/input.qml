import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../../style"
ColumnLayout {
    id: page
    required property var shell
    spacing: 18
    PageTitle { shell: page.shell; title: "Keyboard and pointer" }
    HelpText { shell: page.shell; message: "Keyboard layout" }
    ComboBox {
        model: ["us", "gb", "de", "fr", "es", "jp", "tw"]
        currentIndex: model.indexOf((shell.state.appearance || {}).keyboardLayout || "us")
        onActivated: shell.setAppearance({keyboardLayout: currentText})
        Accessible.name: shell.tr("Keyboard layout")
    }
    PreferenceSlider { shell: page.shell; preference: "keyRepeatRate"; label: "Key repeat rate"; minimum: 0; maximum: 60; suffix: " / s" }
    PreferenceSlider { shell: page.shell; preference: "keyRepeatDelay"; label: "Key repeat delay"; minimum: 200; maximum: 1500; step: 50; suffix: " ms" }
    PreferenceSlider { shell: page.shell; preference: "cursorSize"; label: "Cursor size after session restart"; minimum: 16; maximum: 64; step: 8; suffix: " px" }
    HelpText { shell: page.shell; message: "Pointer speed, natural scrolling and touchpad gestures are supplied by the host in nested sessions. Standalone libinput device controls are not available yet." }
    ToolList { shell: page.shell; category: "input" }
    TextField { Layout.fillWidth: true; placeholderText: shell.tr("Type here to test your keyboard or input method"); color: Theme.text; Accessible.name: placeholderText }
}
