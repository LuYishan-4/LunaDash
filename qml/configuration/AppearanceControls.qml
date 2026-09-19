import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"

ColumnLayout {
    id: controls
    required property var shell
    spacing: 14

    readonly property var appearance: shell.state.appearance || ({})
    readonly property var pins: appearance.colorPins || []

    function addPin(color) {
        let next = controls.pins.slice()
        const normalized = String(color).toLowerCase()
        if (next.indexOf(normalized) < 0)
            next.push(normalized)
        if (next.length > 16)
            next = next.slice(next.length - 16)
        shell.setAppearance({colorPins: next})
    }

    function numericField(field, text, minimum, maximum) {
        const value = Number(text)
        if (!Number.isFinite(value))
            return
        const rounded = Math.round(Math.max(minimum, Math.min(maximum, value)))
        const changes = {}
        changes[field] = rounded
        shell.setAppearance(changes)
    }

    Text {
        text: shell.tr("Primary color")
        color: Theme.text
        font.family: Theme.font
        font.pixelSize: 14
        font.weight: Font.DemiBold
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Used for focus rings, borders, selected controls and active workspace indicators.")
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.WordWrap
    }
    ColorPicker {
        Layout.fillWidth: true
        currentColor: controls.appearance.accent || Theme.defaultAccent
        pins: controls.pins
        pinLabel: shell.tr("Pin")
        onColorCommitted: color => shell.setAppearance({accent: color})
        onPinRequested: color => controls.addPin(color)
    }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

    Text {
        text: shell.tr("Secondary color")
        color: Theme.text
        font.family: Theme.font
        font.pixelSize: 14
        font.weight: Font.DemiBold
    }
    Text {
        Layout.fillWidth: true
        text: shell.tr("Used for panel capsules, status surfaces and larger visual accents.")
        color: Theme.muted
        font.family: Theme.font
        wrapMode: Text.WordWrap
    }
    ColorPicker {
        Layout.fillWidth: true
        currentColor: controls.appearance.secondaryAccent || Theme.defaultSecondaryAccent
        pins: controls.pins
        pinLabel: shell.tr("Pin")
        onColorCommitted: color => shell.setAppearance({secondaryAccent: color})
        onPinRequested: color => controls.addPin(color)
    }

    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.border }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: shell.tr("Window gaps")
            color: Theme.text
            font.family: Theme.font
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            wrapMode: Text.Wrap
        }
        SoftField {
            id: gapField
            Layout.preferredWidth: 92
            text: String(controls.appearance.gap ?? 12)
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 4; top: 32 }
            onAccepted: controls.numericField("gap", text, 4, 32)
            onEditingFinished: if (acceptableInput) controls.numericField("gap", text, 4, 32)
        }
        Text { text: "px"; color: Theme.muted; font.family: Theme.font }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: shell.tr("Panel height")
            color: Theme.text
            font.family: Theme.font
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            wrapMode: Text.Wrap
        }
        SoftField {
            Layout.preferredWidth: 92
            text: String(controls.appearance.panelHeight ?? 40)
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 32; top: 56 }
            onAccepted: controls.numericField("panelHeight", text, 32, 56)
            onEditingFinished: if (acceptableInput) controls.numericField("panelHeight", text, 32, 56)
        }
        Text { text: "px"; color: Theme.muted; font.family: Theme.font }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: shell.tr("Animation duration")
            color: Theme.text
            font.family: Theme.font
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            wrapMode: Text.Wrap
        }
        SoftField {
            Layout.preferredWidth: 92
            text: String(controls.appearance.animationDuration ?? 220)
            inputMethodHints: Qt.ImhDigitsOnly
            validator: IntValidator { bottom: 0; top: 600 }
            onAccepted: controls.numericField("animationDuration", text, 0, 600)
            onEditingFinished: if (acceptableInput) controls.numericField("animationDuration", text, 0, 600)
        }
        Text { text: "ms"; color: Theme.muted; font.family: Theme.font }
    }

    RowLayout {
        Layout.fillWidth: true
        ShellButton {
            Layout.fillWidth: true
            text: shell.tr("Desktop information")
            active: shell.overviewOpen
            onClicked: shell.setAppearance({overview: !shell.overviewOpen})
        }
        ShellButton {
            Layout.fillWidth: true
            text: shell.tr("Show user and host")
            active: controls.appearance.showHostDetails ?? false
            onClicked: shell.setAppearance({showHostDetails: !(controls.appearance.showHostDetails ?? false)})
        }
        Item { Layout.fillWidth: true }
    }
}
