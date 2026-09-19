import "../modules"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Quickshell
import Quickshell.Wayland
import "../components"
import "../style"
import "components" as SettingsComponents
import "../imagepicker"

ModuleSurface {
    id:settings
    moduleId:"settings"
    property string category:"general"
    property var categories:[{id:"general",name:"General"},{id:"appearance",name:"Appearance"},{id:"windows",name:"Windows and workspaces"},{id:"shortcuts",name:"Keyboard shortcuts"},{id:"modules",name:"Shell modules"},{id:"dashboard",name:"Dashboard"},{id:"display",name:"Display"},{id:"input",name:"Keyboard and pointer"},{id:"input-method",name:"Input method"},{id:"sound",name:"Sound"},{id:"network",name:"Internet and network"},{id:"bluetooth",name:"Bluetooth"},{id:"devices",name:"Device manager and disks"},{id:"power",name:"Power and battery"},{id:"privacy",name:"Privacy and accessibility"},{id:"system",name:"Users, date and time"},{id:"applications",name:"Applications and startup"},{id:"about",name:"About LunaDash"}]
    SettingsCatalog{id:catalog}
    readonly property var searchResults:catalog.matches(search.text,shell.tr).filter(result=>settings.categories.some(category=>category.id===result.page))
    readonly property var currentCategory:settings.categories.find(entry=>entry.id===settings.category)||settings.categories[0]
    function showCategory(id){
        if(!settings.categories.some(entry=>entry.id===id))return
        if(category===id && pageLoader.status===Loader.Ready){
            search.clear()
            pageLoader.opacity=1
            pageShift.y=0
            return
        }
        category=id
        search.clear()
        pageLoader.opacity=0
        pageShift.y=12
        pageLoader.setSource(Qt.resolvedUrl("pages/"+id+".qml"),{shell:settings.shell})
    }
    function openResult(entry){showCategory(entry.page)}
    readonly property int overlayMargin:Math.max(8,Math.min(moduleMargin,24))
    readonly property bool updateAuthorizing:(shell.updateInstall || {}).state==="running" && (shell.updateInstall || {}).stage==="authorization"
    readonly property int configuredX:Number.isFinite(Number(moduleStyle.x))?Number(moduleStyle.x):0
    readonly property int configuredY:Number.isFinite(Number(moduleStyle.y))?Number(moduleStyle.y):0
    anchors.top:true;anchors.left:true
    margins.left:configuredX===0?overlayMargin:configuredX
    margins.top:configuredY===0?Theme.barHeight+overlayMargin:configuredY
    implicitWidth:moduleWidth(1120);implicitHeight:moduleHeight(720)
    exclusionMode:ExclusionMode.Ignore
    WlrLayershell.layer:updateAuthorizing?WlrLayer.Bottom:WlrLayer.Overlay;WlrLayershell.namespace:"lunadash-settings";WlrLayershell.keyboardFocus:opened&&!updateAuthorizing?WlrKeyboardFocus.Exclusive:WlrKeyboardFocus.None
    color:"transparent"
    Rectangle{anchors.fill:parent;color:moduleBackground;border.color:Theme.border;radius:moduleRadius}
    ColumnLayout {
        anchors.fill:parent;anchors.margins:24;spacing:20
        RowLayout{
            spacing:12
            SettingsComponents.PageTitle{shell:settings.shell;title:"Settings";color:moduleForeground;font.pixelSize:23;Layout.fillWidth:true}
            Rectangle{
                Layout.maximumWidth:Math.max(100,settings.width*0.35);implicitWidth:categoryBadge.implicitWidth+48;implicitHeight:30;radius:15
                color:Qt.rgba(moduleAccent.r,moduleAccent.g,moduleAccent.b,0.10)
                border.width:1;border.color:Qt.rgba(moduleAccent.r,moduleAccent.g,moduleAccent.b,0.30)
                Row{
                    anchors.centerIn:parent;width:parent.width-26;spacing:7
                    LineIcon{name:settings.currentCategory.id;width:15;height:15;ink:moduleAccent;anchors.verticalCenter:parent.verticalCenter}
                    Text{id:categoryBadge;width:Math.max(0,parent.width-22);elide:Text.ElideRight;text:shell.tr(settings.currentCategory.name);color:moduleForeground;font.family:Theme.font;font.pixelSize:11;font.weight:Font.DemiBold}
                }
                Behavior on implicitWidth{NumberAnimation{duration:Theme.motionFast;easing.type:Easing.OutCubic}}
            }
            Text{text:"LunaDash";color:moduleAccent;font.family:Theme.font;font.pixelSize:12}
            ShellButton{text:"×";quiet:true;toolTip:shell.tr("Quick hide settings");Accessible.name:shell.tr("Quick hide settings");onClicked:shell.settingsOpen=false}
        }
        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:24
            ColumnLayout {
                Layout.preferredWidth:Math.min(258,settings.width*0.28);Layout.minimumWidth:140;Layout.maximumWidth:258;Layout.fillHeight:true;spacing:12
                SoftField {
                    id:search;Layout.fillWidth:true;implicitHeight:38;leftPadding:36;placeholderText:shell.tr("Search settings");Accessible.name:placeholderText
                    LineIcon{name:"search";width:17;height:17;anchors.left:parent.left;anchors.leftMargin:11;anchors.verticalCenter:parent.verticalCenter}
                    Keys.onDownPressed:{if(search.text.length>0&&resultList.count>0){resultList.currentIndex=0;resultList.forceActiveFocus()}else if(categoryList.count>0){categoryList.currentIndex=0;categoryList.forceActiveFocus()}}
                    Keys.onEscapePressed:shell.settingsOpen=false
                }
                Text{ wrapMode: Text.Wrap; Layout.minimumWidth: 0;
                    visible:search.text.trim().length>0
                    Layout.fillWidth:true
                    text:resultList.count+" "+shell.tr(resultList.count===1?"result":"results")
                    color:Theme.muted;font.family:Theme.font;font.pixelSize:10
                    opacity:visible?1:0
                    Behavior on opacity{NumberAnimation{duration:Theme.motionFast}}
                }
                ListView {
                    id:resultList;visible:search.text.trim().length>0;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:3;model:settings.searchResults;currentIndex:count>0?0:-1;keyNavigationWraps:true;Accessible.name:shell.tr("Setting search results");ScrollBar.vertical:ScrollBar{}
                    delegate:SearchResultDelegate{required property var modelData;required property int index;entry:modelData;shell:settings.shell;highlighted:ListView.isCurrentItem;onClicked:settings.openResult(entry);Keys.onReturnPressed:settings.openResult(entry);Keys.onEnterPressed:settings.openResult(entry);Keys.onSpacePressed:settings.openResult(entry);Keys.onEscapePressed:{search.forceActiveFocus();search.selectAll()}}
                    Keys.onUpPressed:event=>{if(currentIndex<=0){search.forceActiveFocus();event.accepted=true}else{currentIndex--;event.accepted=true}}
                    Keys.onDownPressed:event=>{if(count>0){currentIndex=(currentIndex+1)%count;event.accepted=true}}
                }
                Text{ Layout.minimumWidth: 0;visible:resultList.visible&&resultList.count===0;Layout.fillWidth:true;text:shell.tr("No settings found");color:Theme.muted;font.family:Theme.font;wrapMode:Text.WordWrap}
                ListView {
                    id:categoryList;visible:!resultList.visible;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:3;model:settings.categories;keyNavigationWraps:true;ScrollBar.vertical:ScrollBar{}
                    delegate:Rectangle {
                        id:categoryRow;required property var modelData;required property int index;readonly property bool selected:settings.category===modelData.id;width:ListView.view.width-12;height:Math.max(38,categoryLabel.implicitHeight+16);radius:10;color:selected?Qt.rgba(settings.moduleAccent.r,settings.moduleAccent.g,settings.moduleAccent.b,0.14):categoryMouse.containsMouse?Theme.controlHover:"transparent";scale:categoryMouse.pressed?0.985:categoryMouse.containsMouse?1.008:1;activeFocusOnTab:true;border.width:activeFocus?1:0;border.color:settings.moduleAccent;Accessible.role:Accessible.Button;Accessible.name:shell.tr(modelData.name)
                        Keys.onReturnPressed:settings.showCategory(modelData.id);Keys.onEnterPressed:settings.showCategory(modelData.id);Keys.onSpacePressed:settings.showCategory(modelData.id);Keys.onEscapePressed:search.forceActiveFocus()
                        Row{anchors.left:parent.left;anchors.right:parent.right;anchors.margins:12;anchors.verticalCenter:parent.verticalCenter;spacing:12;LineIcon{name:modelData.id;width:19;height:19;ink:categoryRow.selected?settings.moduleAccent:Theme.muted}Text{id:categoryLabel;width:Math.max(0,parent.width-31);wrapMode:Text.Wrap;text:shell.tr(modelData.name);color:categoryRow.selected?settings.moduleAccent:Theme.text;font.pixelSize:12;font.family:Theme.font;anchors.verticalCenter:parent.verticalCenter}}
                        Rectangle{visible:categoryRow.selected;width:3;height:15;radius:1.5;color:settings.moduleAccent;anchors.left:parent.left;anchors.verticalCenter:parent.verticalCenter}
                        MouseArea{id:categoryMouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:settings.showCategory(modelData.id)}
                        Behavior on color{ColorAnimation{duration:Theme.motionFast}}
                        Behavior on scale{NumberAnimation{duration:Theme.motionFast;easing.type:Easing.OutCubic}}
                    }
                    Keys.onUpPressed:event=>{if(currentIndex<=0){search.forceActiveFocus();event.accepted=true}else{currentIndex--;event.accepted=true}}
                    Keys.onDownPressed:event=>{if(count>0){currentIndex=(currentIndex+1)%count;event.accepted=true}}
                }
            }
            Rectangle{Layout.fillHeight:true;width:1;color:Theme.border}
            Rectangle {
                Layout.fillWidth:true;Layout.fillHeight:true;Layout.minimumWidth:0;radius:moduleRadius;color:Qt.rgba(moduleBackground.r,moduleBackground.g,moduleBackground.b,0.94)
                ScrollView{id:scroll;anchors.fill:parent;anchors.margins:24;clip:true;contentWidth:availableWidth
                    Loader{
                        id:pageLoader
                        width:scroll.availableWidth-12
                        height:item?item.implicitHeight:0
                        transform:Translate{id:pageShift;y:0}
                        onStatusChanged:if(status===Loader.Error)console.warn("Settings page failed to load: "+settings.category)
                        onLoaded:{
                            scroll.contentItem.contentY=0
                            Qt.callLater(function(){if(scroll.contentItem)scroll.contentItem.contentY=0})
                            pageEnter.restart()
                        }
                    }
                    ParallelAnimation{
                        id:pageEnter
                        NumberAnimation{target:pageLoader;property:"opacity";from:0;to:1;duration:Theme.motion;easing.type:Easing.OutCubic}
                        NumberAnimation{target:pageShift;property:"y";from:12;to:0;duration:Theme.motion;easing.type:Easing.OutCubic}
                    }
                }
            }
        }
    }
    ImagePicker{anchors.fill:parent;cornerRadius:settings.moduleRadius;shell:settings.shell;opened:settings.shell.pickerOpen;onClosed:settings.shell.pickerOpen=false}
    onOpenedChanged: {
        if (opened) {
            Qt.callLater(function() {
                if (!settings.opened)
                    return
                search.forceActiveFocus(Qt.OtherFocusReason)
                search.prepareInputMethod()
            })
        } else {
            search.focus = false
            resultList.focus = false
            categoryList.focus = false
        }
    }
    Component.onCompleted:showCategory(shell.settingsPage || "general")
}
