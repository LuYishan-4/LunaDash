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
    readonly property var system: shell.state.system || ({})
    readonly property var update: shell.state.update || ({status:"idle",currentVersion:"0.1.0",channel:"stable"})
    spacing:16
    PageTitle { shell:page.shell; title:"About LunaDash" }
    SettingsComponents.SettingsCard { title:""; RowLayout { Layout.fillWidth:true; spacing:22; LunaDashLogo { Layout.preferredWidth:126; Layout.preferredHeight:126; animated:Theme.animations } ColumnLayout { Layout.fillWidth:true; spacing:6; Text { text:"LunaDash "+(page.update.currentVersion||"0.1.0"); color:Theme.text; font.family:Theme.font; font.pixelSize:28; font.bold:true } Text { text:shell.tr("A moonlit, focused Linux desktop."); color:Theme.muted; font.family:Theme.font; font.pixelSize:13 } Text { visible:Boolean(page.update.currentCommit); text:(page.update.channel==="dev"?"dev · ":"")+String(page.update.currentCommit||"").slice(0,12); color:Theme.muted; font.pixelSize:10 } } } }
    SettingsComponents.SettingsCard { title:shell.tr("Community and source"); description:shell.tr("Follow development, report issues, and review the source code."); ShellButton { text:"GitHub"; onClicked:Qt.openUrlExternally(page.update.repositoryUrl||"https://github.com/LuYishan-4/LunaDash") } }
    SettingsComponents.SettingsCard {
        title:shell.tr("Software updates")
        description:page.update.channel==="dev"?shell.tr("Development follows the latest commit on the dev branch."):shell.tr("Stable follows published GitHub Releases from the main branch.")
        RowLayout { Layout.fillWidth:true; Text { text:shell.tr("Channel"); color:Theme.muted } ComboBox { model:[shell.tr("Stable (main / releases)"),shell.tr("Development (dev / commits)")]; currentIndex:page.update.channel==="dev"?1:0; onActivated:shell.command("update-channel",currentIndex===1?"dev":"stable") } Item { Layout.fillWidth:true } ShellButton { text:shell.tr("Check now"); enabled:page.update.status!=="checking"; onClicked:shell.command("check-update","") } }
        RowLayout { Layout.fillWidth:true; LineIcon { width:24;height:24;name:"update";ink:page.update.status==="available"?Theme.accent:Theme.muted } ColumnLayout { Layout.fillWidth:true; Text { Layout.fillWidth:true; wrapMode:Text.WordWrap; color:Theme.text; text:page.update.status==="checking"?shell.tr("Checking for updates…"):page.update.status==="available"?(page.update.channel==="dev"?shell.tr("A newer development commit is available: ")+String(page.update.latestCommit||"").slice(0,12):shell.tr("A new upstream release is available: ")+page.update.latestVersion):page.update.status==="upToDate"?(page.update.channel==="dev"?shell.tr("LunaDash is up to date with the dev branch."):shell.tr("LunaDash is up to date with the latest release.")):page.update.status==="error"?shell.tr(page.update.error||"The update check failed."):shell.tr("No update check has been run yet.") } Text { visible:page.update.channel==="dev"&&Boolean(page.update.latestMessage); text:page.update.latestMessage||""; color:Theme.muted; font.pixelSize:10; elide:Text.ElideRight; Layout.fillWidth:true } Text { visible:Boolean(page.update.checkedAt); text:shell.tr("Last checked: ")+page.update.checkedAt; color:Theme.muted; font.pixelSize:10 } } ShellButton { visible:page.update.status==="available"&&Boolean(page.update.releaseUrl); text:page.update.channel==="dev"?shell.tr("View commit"):shell.tr("View release"); onClicked:Qt.openUrlExternally(page.update.releaseUrl) } }
    }
    SettingsComponents.SettingsCard { title:shell.tr("System information"); GridLayout { Layout.fillWidth:true; columns:2; Text{text:shell.tr("Operating system");color:Theme.muted} Text{Layout.fillWidth:true;text:page.system.os||"Linux";color:Theme.text} Text{text:shell.tr("Kernel and architecture");color:Theme.muted} Text{Layout.fillWidth:true;text:(page.system.kernel||"—")+" · "+(page.system.architecture||"—");color:Theme.text} Text{text:shell.tr("Graphics API");color:Theme.muted} Text{Layout.fillWidth:true;text:(shell.state.graphicsApi||"OpenGL")+" "+(shell.state.graphicsMajor||0)+"."+(shell.state.graphicsMinor||0);color:Theme.text} } }
}
