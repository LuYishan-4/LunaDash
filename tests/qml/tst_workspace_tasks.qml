import QtQuick
import QtTest
import "../../qml/columns/WorkspaceTasks.js" as WorkspaceTasks

TestCase {
    name: "WorkspaceTasks"
    function test_groups_all_windows_and_keeps_hidden_tasks() {
        const clients = []
        for (let id = 1; id <= 12; ++id)
            clients.push({id: id, workspace: id === 12 ? 2 : 0, mapped: true,
                          minimized: id === 3, hiddenByMaximize: id !== 2})
        clients.push({id: 20, workspace: 0, mapped: false})
        clients.push({id: 21, workspace: 0, mapped: true, desktop: true})
        const groups = WorkspaceTasks.groupByWorkspace(clients, 2)
        compare(groups.length, 2)
        compare(groups[0].members.length, 11)
        compare(groups[0].members[2].window, 3)
        verify(groups[0].members[2].minimized)
        verify(!groups[0].active)
        verify(groups[1].active)
        compare(groups[1].members[0].window, 12)
    }
    function test_workspace_color_follows_current_workspace() {
        const clients = [{id:1, workspace:4, mapped:true, focused:true},
                         {id:2, workspace:1, mapped:true}]
        let groups = WorkspaceTasks.groupByWorkspace(clients, 1)
        compare(groups[0].workspace, 1)
        verify(groups[0].active)
        verify(!groups[1].active)
        groups = WorkspaceTasks.groupByWorkspace(clients, 4)
        verify(!groups[0].active)
        verify(groups[1].active)
    }
}
