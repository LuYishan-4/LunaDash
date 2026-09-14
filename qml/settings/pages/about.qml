import QtQuick
import QtQuick.Layouts
import "../components"
import "../../components"
import "../../style"

ColumnLayout {
    id: page
    required property var shell
    readonly property var system: shell.state.system || ({})
    spacing: 18

    PageTitle { shell: page.shell; title: "About LunaDah" }
    RowLayout {
        spacing: 18
        BrandIcon { shell: page.shell; Layout.preferredWidth: 112; Layout.preferredHeight: 84 }
        ColumnLayout {
            Layout.fillWidth: true
            Text { text: "LunaDah 0.1"; color: Theme.text; font.family: Theme.font; font.pixelSize: 24 }
            Text { text: shell.tr("A quiet, focused Linux desktop."); color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
            Text { text: shell.tr("Development preview"); color: Theme.accent; font.family: Theme.font; font.pixelSize: 12 }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: 24
        rowSpacing: 8
        Text { text: shell.tr("Operating system"); color: Theme.muted }
        Text { Layout.fillWidth: true; text: page.system.os || "Linux"; color: Theme.text; elide: Text.ElideRight }
        Text { text: shell.tr("Kernel and architecture"); color: Theme.muted }
        Text { Layout.fillWidth: true; text: (page.system.kernel || "—") + "  ·  " + (page.system.architecture || "—"); color: Theme.text; elide: Text.ElideRight }
        Text { text: shell.tr("Graphics API"); color: Theme.muted }
        Text { Layout.fillWidth: true; text: (shell.state.graphicsApi || "OpenGL") + " " + (shell.state.graphicsMajor || 0) + "." + (shell.state.graphicsMinor || 0); color: Theme.text }
        Text { text: shell.tr("Session"); color: Theme.muted }
        Text { Layout.fillWidth: true; text: shell.tr("Wayland compositor with a Quickshell interface"); color: Theme.text; wrapMode: Text.WordWrap }
    }

    Text { text: shell.tr("Included features"); color: Theme.accent; font.family: Theme.font; font.pixelSize: 15 }
    HelpText { shell: page.shell; message: "C11 rendering and data cores, C++20 Wayland integration, and a Quickshell interface." }
    HelpText { shell: page.shell; message: "Tiling workspaces, desktop applications, configurable shell modules, system tray integration, and host settings tools are available." }

    Text { text: shell.tr("Current limitations"); color: Theme.accent; font.family: Theme.font; font.pixelSize: 15 }
    HelpText { shell: page.shell; message: "This is a development preview. Available settings depend on the implemented compositor features, installed tools and running system services." }
    HelpText { shell: page.shell; message: "Secure screen locking, notification service, session restoration, standalone multi-monitor controls, HDR, and color management are not available yet." }
    HelpText { shell: page.shell; message: "The Fcitx status icon uses the system tray. Input-method candidate popup compatibility is separate and remains unverified; no candidate-popup compositor protocol support is claimed." }
    HelpText { shell: page.shell; message: "English documentation: docs/SETTINGS.md and docs/TESTING_AND_FILES.md in the source tree." }
}
