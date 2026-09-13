import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"
ColumnLayout {
    id: effects
    required property var shell
    property var preferences: shell.state.appearance || ({})
    spacing: 10
    Text { text: shell.tr("Glass and motion"); color: Theme.accent; font.pixelSize: 16; font.weight: Font.Medium }
    RowLayout {
        SoftSwitch { text: shell.tr("Background blur"); checked: effects.preferences.blur ?? true; onToggled: shell.setAppearance({ blur: !(effects.preferences.blur ?? true) }) }
        SoftSwitch { text: shell.tr("Animations"); checked: effects.preferences.animations ?? true; onToggled: shell.setAppearance({ animations: !(effects.preferences.animations ?? true) }) }
    }
    Repeater {
        model: [
            { key: "blurRadius", label: "Blur strength", minimum: 0, maximum: 32, step: 2, fallback: 18, suffix: " px" },
            { key: "windowOpacity", label: "Window opacity", minimum: 60, maximum: 100, step: 2, fallback: 96, suffix: "%" },
            { key: "animationDuration", label: "Animation duration", minimum: 0, maximum: 600, step: 20, fallback: 220, suffix: " ms" }
        ]
        ColumnLayout {
            required property var modelData
            Layout.fillWidth: true; spacing: 2
            RowLayout {
                Text { text: shell.tr(modelData.label); color: Theme.muted; Layout.fillWidth: true }
                Text { text: (effects.preferences[modelData.key] ?? modelData.fallback) + modelData.suffix; color: Theme.text }
            }
            SoftSlider {
                Layout.fillWidth: true; from: modelData.minimum; to: modelData.maximum; stepSize: modelData.step
                id: slider
                Binding on value { value: effects.preferences[modelData.key] ?? modelData.fallback; when: !slider.pressed }
                onPressedChanged: if (!pressed) { const change = {}; change[modelData.key] = Math.round(value); shell.setAppearance(change) }
                Keys.onReleased: event => { if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) { const change = {}; change[modelData.key] = Math.round(value); shell.setAppearance(change) } }
                Accessible.name: shell.tr(modelData.label)
            }
        }
    }
    Text { text: shell.tr("Turn animations off for reduced motion. Lower blur strength to reduce GPU work."); color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 11 }
}
