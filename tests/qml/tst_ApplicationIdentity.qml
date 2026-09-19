import QtQuick
import QtTest
import "../../qml/components/ApplicationIdentity.js" as Identity

TestCase {
    name: "ApplicationIdentity"
    function test_desktop_identity() {
        const entries = [
            {id: "org.mozilla.firefox", startupClass: "firefox", icon: "firefox"},
            {id: "com.discordapp.Discord", startupClass: "discord", icon: "com.discordapp.Discord"},
            {id: "org.example.Code", startupClass: "Code", icon: "org.example.Code"},
            {id: "org.example.Terminal", startupClass: "shared", icon: "terminal"},
            {id: "org.example.Files", startupClass: "shared", icon: "files"}
        ]
        compare(Identity.desktopEntry("org.mozilla.firefox.desktop", entries).icon, "firefox")
        compare(Identity.desktopEntry("Discord", entries).id, "com.discordapp.Discord")
        compare(Identity.desktopEntry("org.example.code", entries).icon, "org.example.Code")
        compare(Identity.desktopEntry("Terminal settings in Firefox", entries), null)
        compare(Identity.desktopEntry("shared", entries), null, "Ambiguous startup classes must not choose an arbitrary app")
        compare(Identity.desktopEntry("", entries), null)
        verify(Identity.isThemeIconName("org.example.Code"))
        verify(Identity.isThemeIconName("org.kde.kate"))
        verify(!Identity.isThemeIconName("missing.png"))
        verify(!Identity.isThemeIconName("/usr/bin/program"))
        compare(Identity.fileSource("/opt/application/icon.svg"), "file:///opt/application/icon.svg")
        compare(Identity.vectorName("org.example.file-editor"), "apps")
    }
}
