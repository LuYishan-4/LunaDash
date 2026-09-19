import "../modules"
import QtQuick
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
ModuleSurface {
    id: panel
    moduleId: "compatibility"
    implicitWidth: moduleWidth(520); implicitHeight: moduleHeight(270)
    exclusionMode: ExclusionMode.Ignore
    WlrLayershell.layer: WlrLayer.Overlay
    WlrLayershell.namespace: "lunadash-x11-launcher"
    WlrLayershell.keyboardFocus: opened ? WlrKeyboardFocus.Exclusive : WlrKeyboardFocus.None
    color: "transparent"
    onOpenedChanged: {
        if (opened) {
            Qt.callLater(function() {
                if (!panel.opened)
                    return
                command.forceActiveFocus(Qt.OtherFocusReason)
                command.prepareInputMethod()
            })
        } else {
            command.focus = false
        }
    }
    Timer { id:dismissTimer; interval:2000; running:panel.opened; repeat:false; onTriggered:if(!panelMouse.containsMouse)shell.x11Open=false }
    MouseArea { id:panelMouse; anchors.fill:parent; hoverEnabled:true; acceptedButtons:Qt.NoButton; onEntered:dismissTimer.stop(); onExited:dismissTimer.restart(); onPositionChanged:dismissTimer.stop() }
    Rectangle { anchors.fill:parent; color:moduleBackground; radius:moduleRadius }
    ColumnLayout {
        anchors.fill:parent; anchors.margins:28; spacing:15
        RowLayout { Text{text:shell.tr("Run an X11 application");color:moduleForeground;font.family:Theme.font;font.pixelSize:22;Layout.fillWidth:true} ShellButton{text:"×";onClicked:shell.x11Open=false} }
        Text { text:(shell.state.xwayland||{}).available?shell.tr("X11 apps open inside a compatibility window"):((shell.state.xwayland||{}).error||shell.tr("XWayland is unavailable"));color:Theme.muted;font.family:Theme.font;wrapMode:Text.WordWrap;Layout.fillWidth:true }
        SoftField { id:command;Layout.fillWidth:true;placeholderText:shell.tr("Program and arguments");onAccepted:run.clicked();focus:true }
        Text { text:shell.tr("Enter a program and arguments. Shell operators are not evaluated.");color:Theme.muted;font.family:Theme.font;font.pixelSize:11;wrapMode:Text.WordWrap;Layout.fillWidth:true }
        ShellButton { id:run;text:shell.tr("Launch");active:true;enabled:command.text.trim().length>0&&Boolean((shell.state.xwayland||{}).available);onClicked:{shell.command("launch-x11",command.text);shell.x11Open=false} }
    }
}
