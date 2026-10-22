// Compare effective target enablement, not changing metrics or error messages.
function enabledTargets(state) {
    const result = {};
    (state.installed || []).forEach(entry => {
        const id = String(entry.id || entry.packageId || "");
        if (!id.length) return;
        if (!result[id]) result[id] = {name: entry.name || id, targets: []};
        if (entry.enabled) result[id].targets.push(String(entry.target || ""));
    });
    Object.keys(result).forEach(id => result[id].targets.sort());
    return result;
}
function changes(previous, next) {
    const before = enabledTargets(previous), after = enabledTargets(next);
    const ids = [...new Set(Object.keys(before).concat(Object.keys(after)))].sort();
    return ids.filter(id => JSON.stringify(before[id]?.targets || []) !==
                            JSON.stringify(after[id]?.targets || []))
        .map(id => ({id: id, name: after[id]?.name || before[id]?.name || id,
            enabled: Boolean(after[id]?.targets.length),
            targets: after[id]?.targets || [], previousTargets: before[id]?.targets || []}));
}
