import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../style"
ColumnLayout {
    id: controls
    required property var shell
    spacing: 10
    Text { text: shell.tr("Accent color"); color: Theme.muted; font.family: Theme.font }
    RowLayout { spacing:14
        Repeater { model:["#9ccbfb","#c4b5fd","#7dcccf","#e7b899"]
            Rectangle { required property string modelData; required property int index; implicitWidth:46;implicitHeight:46;radius:15;color:modelData;border.width:Theme.accent.toString()===modelData?3:0;border.color:Theme.focusRing;activeFocusOnTab:true;Accessible.role:Accessible.Button;Accessible.name:shell.tr(["Sky","Lavender","Mint","Peach"][index]);Keys.onReturnPressed:shell.setAppearance({accent:modelData});Text{anchors.centerIn:parent;visible:Theme.accent.toString()===modelData;text:"✓";color:Theme.accentInk;font.family:Theme.font;font.pixelSize:20}MouseArea{anchors.fill:parent;cursorShape:Qt.PointingHandCursor;onClicked:shell.setAppearance({accent:modelData})} }
        }
        Item { Layout.fillWidth:true }
    }
    RowLayout { Layout.fillWidth:true
        SoftField { id:customAccent;Layout.fillWidth:true;text:(shell.state.appearance||{}).accent||Theme.defaultAccent;placeholderText:"#RRGGBB";invalid:!acceptableInput;font.family:Theme.font;inputMethodHints:Qt.ImhNoPredictiveText;validator:RegularExpressionValidator{regularExpression:/^#[0-9a-fA-F]{6}$/};onAccepted:if(acceptableInput)shell.setAppearance({accent:text.toLowerCase()}) }
        Rectangle { implicitWidth:36;implicitHeight:36;radius:10;color:customAccent.acceptableInput?customAccent.text:Theme.accent;border.color:Theme.border }
        ShellButton { text:shell.tr("Apply");enabled:customAccent.acceptableInput&&customAccent.text.toLowerCase()!==((shell.state.appearance||{}).accent||"").toLowerCase();onClicked:shell.setAppearance({accent:customAccent.text.toLowerCase()}) }
    }
    RowLayout { Text{text:shell.tr("Window gaps")+"  "+((shell.state.appearance||{}).gap??12)+" px";color:Theme.text;font.family:Theme.font;Layout.fillWidth:true} ShellButton{text:"−";enabled:((shell.state.appearance||{}).gap??12)>4;onClicked:shell.setAppearance({gap:Math.max(4,((shell.state.appearance||{}).gap??12)-4)})} ShellButton{text:"+";enabled:((shell.state.appearance||{}).gap??12)<32;onClicked:shell.setAppearance({gap:Math.min(32,((shell.state.appearance||{}).gap??12)+4)})} }
    RowLayout { Text{text:shell.tr("Panel height")+"  "+((shell.state.appearance||{}).panelHeight??40)+" px";color:Theme.text;font.family:Theme.font;Layout.fillWidth:true} ShellButton{text:"−";enabled:((shell.state.appearance||{}).panelHeight??40)>32;onClicked:shell.setAppearance({panelHeight:Math.max(32,((shell.state.appearance||{}).panelHeight??40)-4)})} ShellButton{text:"+";enabled:((shell.state.appearance||{}).panelHeight??40)<56;onClicked:shell.setAppearance({panelHeight:Math.min(56,((shell.state.appearance||{}).panelHeight??40)+4)})} }
    RowLayout { ShellButton{text:shell.tr("Desktop information");active:shell.overviewOpen;onClicked:shell.setAppearance({overview:!shell.overviewOpen})} ShellButton{text:shell.tr("Show user and host");active:(shell.state.appearance||{}).showHostDetails??false;onClicked:shell.setAppearance({showHostDetails:!((shell.state.appearance||{}).showHostDetails??false)})} }
}
