.pragma library

function groupByWorkspace(clients, currentWorkspace) {
    const workspaces = {}
    for (const client of clients || []) {
        if (!client.mapped || client.desktop || client.utility)
            continue
        const workspace = Number(client.workspace || 0)
        if (!workspaces[workspace])
            workspaces[workspace] = {workspace: workspace,
                active: workspace === Number(currentWorkspace), members: []}
        workspaces[workspace].members.push({window: client.id, title: client.title,
            appId: client.appId, icon: client.icon, minimized: client.minimized,
            focused: client.focused, maximized: client.maximized})
    }
    return Object.keys(workspaces).map(key => workspaces[key])
        .sort((left, right) => left.workspace - right.workspace)
}
