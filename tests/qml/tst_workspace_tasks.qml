import QtQuick
import QtTest
import "../../qml/columns/WorkspaceTasks.js" as WorkspaceTasks

TestCase {
    name: "WorkspaceTasks"
    function test_spare_workspace_survives_switching_back() {
        compare(WorkspaceTasks.visibleWorkspaces([], 0, 10, true), [0, 1])
        compare(WorkspaceTasks.visibleWorkspaces([], 1, 10, true), [0, 1])
        const clients = [{workspace: 0, mapped: true},
                         {workspace: 1, mapped: true, visible: false, minimized: true}]
        compare(WorkspaceTasks.visibleWorkspaces(clients, 1, 10, true), [0, 1, 2])
        compare(WorkspaceTasks.visibleWorkspaces(clients, 0, 10, true), [0, 1, 2])
        compare(WorkspaceTasks.visibleWorkspaces(clients, 2, 10, true), [0, 1, 2])
        clients.pop()
        compare(WorkspaceTasks.visibleWorkspaces(clients, 0, 10, true), [0, 1])
    }
    function test_workspace_gaps_and_limits() {
        const clients = [{workspace: 3, mapped: true},
                         {workspace: 8, mapped: false},
                         {workspace: 7, mapped: true, utility: true},
                         {workspace: 6, mapped: true, desktop: true}]
        compare(WorkspaceTasks.visibleWorkspaces(clients, 0, 10, true), [0, 1, 2, 3, 4])
        compare(WorkspaceTasks.visibleWorkspaces(clients, 0, 4, true), [0, 1, 2, 3])
        compare(WorkspaceTasks.visibleWorkspaces([], 0, 1, true), [0])
        compare(WorkspaceTasks.visibleWorkspaces([], 3, 5, true), [0, 1, 2, 3])
        compare(WorkspaceTasks.visibleWorkspaces([], 0, 4, false), [0, 1, 2, 3])
    }
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
