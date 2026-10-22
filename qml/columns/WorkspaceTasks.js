.pragma library

function visibleWorkspaces(clients, currentWorkspace, configuredCount, compact) {
    const current = Math.max(0, Number(currentWorkspace) || 0)
    const count = Math.max(1, Number(configuredCount) || 10, current + 1)
    if (!compact)
        return Array.from({length: count}, (_, index) => index)
    let lastOccupied = -1
    for (const client of clients || []) {
        if (client.mapped && !client.desktop && !client.utility)
            lastOccupied = Math.max(lastOccupied, Number(client.workspace) || 0)
    }
    // Keep a contiguous strip and one spare after the last occupied workspace.
    // Visiting the spare does not make it occupied or create another spare.
    const last = Math.min(count - 1, Math.max(1, current, lastOccupied + 1))
    return Array.from({length: last + 1}, (_, index) => index)
}

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
