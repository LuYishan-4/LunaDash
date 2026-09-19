import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var output: shell.state.display || ({})
    readonly property var backlight: shell.state.brightness || ({})
    property int pendingBrightness: -1
    readonly property var scales: [
        {label:"100%", value:1}, {label:"125%", value:1.25},
        {label:"150%", value:1.5}, {label:"175%", value:1.75},
        {label:"200%", value:2}, {label:"250%", value:2.5}, {label:"300%", value:3}
    ]
    spacing: 16

    PageTitle { shell: page.shell; title: "Display" }
    SettingsCard {
        title: shell.tr("Display information")
        description: (page.output.output || "") + " · " + (page.output.pixelWidth || 0) + " × " + (page.output.pixelHeight || 0)
        HelpText { shell: page.shell; message: Math.round((page.output.scale || 1) * 100) + "% · " + (page.output.refreshRate || 0).toFixed(2) + " Hz" }
    }
    SettingsCard {
        title: shell.tr("Brightness")
        description: page.backlight.available ? shell.tr("Adjust the active backlight device.") + " " + page.backlight.device
            : shell.tr("No controllable backlight device was detected. brightnessctl is required for laptop/internal-panel brightness control.")
        RowLayout {
            Layout.fillWidth: true
            Text { Layout.fillWidth:true; Layout.minimumWidth:0; wrapMode:Text.Wrap; text:shell.tr("Screen brightness"); color:Theme.text; font.family:Theme.font }
            Text { text:page.backlight.available ? Math.round(brightness.value)+"%" : "—"; color:Theme.muted; font.family:Theme.font }
        }
        SoftSlider {
            id: brightness
            Layout.fillWidth: true
            from: 1; to: 100; stepSize: 1
            value: page.backlight.percent || 1
            enabled: page.backlight.available || false
            onMoved: { page.pendingBrightness = Math.round(value); brightnessDebounce.restart() }
        }
        HelpText { shell: page.shell; visible: Boolean(page.backlight.error); message: page.backlight.error || "" }
    }
    SettingsCard {
        title: shell.tr("Resolution and refresh rate")
        description: page.output.nested ? shell.tr("Resize the LunaDash window while running as a nested compositor.") : shell.tr("Choose a mode supported by the active display.")
        StyledComboBox {
            Layout.fillWidth: true
            translationContext: page.shell
            model: page.output.modes || []
            textRole: "label"; valueRole: "id"
            currentIndex: model.findIndex(entry => entry.id === page.output.mode)
            enabled: count > 0 && !page.output.pending
            onActivated: shell.command("display-configure", JSON.stringify({mode: currentValue}))
        }
    }
    SettingsCard {
        title: shell.tr("Display scale")
        description: shell.tr("Change the size of text and controls on the active display.")
        StyledComboBox {
            Layout.fillWidth: true
            translationContext: page.shell
            model: page.scales
            textRole: "label"; valueRole: "value"
            currentIndex: page.scales.findIndex(entry => Math.abs(entry.value - (page.output.scale || 1)) < 0.01)
            enabled: !page.output.pending
            onActivated: shell.command("display-configure", JSON.stringify({scale: currentValue}))
        }
    }
    SettingsCard {
        visible: page.output.pending || false
        emphasized: true
        title: shell.tr("Keep these display settings?")
        description: shell.tr("The previous settings will be restored automatically.") + " " + (page.output.revertSeconds || 0) + " s"
        RowLayout {
            Layout.fillWidth: true
            ShellButton { Layout.fillWidth:true; text:shell.tr("Keep changes"); active:true; onClicked:shell.command("display-confirm", "") }
            ShellButton { Layout.fillWidth:true; text:shell.tr("Revert"); onClicked:shell.command("display-revert", "") }
        }
    }
    HelpText { shell: page.shell; visible: Boolean(page.output.error); message: page.output.error || "" }
    HelpText { shell: page.shell; visible: page.output.nested || false; message: "In a nested session, physical monitor resolution and refresh rate are controlled by your host desktop." }
    HelpText { shell: page.shell; message: "External monitors usually require DDC/CI rather than the kernel backlight interface, so they may not appear here." }
    Timer {
        id: brightnessDebounce
        interval: 120
        onTriggered: {
            if (page.pendingBrightness < 0) return
            page.shell.command("brightness", String(page.pendingBrightness))
            page.pendingBrightness = -1
        }
    }
}
