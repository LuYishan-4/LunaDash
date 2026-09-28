// Pure gallery policy, shared by the popup and its regression tests.
function visibleEntries(entries, directory, source, category, kind, query) {
    const folder = String(directory || "").replace(/\/+$/, "");
    const prefix = folder + "/";
    const needle = String(query || "").trim().toLocaleLowerCase();
    const seen = new Set();
    return (entries || []).filter(entry => {
        if (!entry || !entry.path || seen.has(entry.path)) return false;
        seen.add(entry.path);
        if (source === "library" && (!folder || !String(entry.path).startsWith(prefix))) return false;
        if (source === "bundled" && !entry.bundled) return false;
        if (source === "favorites" && !entry.favorite) return false;
        return (!category || entry.category === category)
            && (kind === "all" || entry.type === kind)
            && String(entry.name + " " + entry.category).toLocaleLowerCase().includes(needle);
    });
}
function categories(entries) {
    return [...new Set((entries || []).map(entry => entry.category).filter(Boolean))].sort();
}
function nextIndex(index, delta, count) {
    return count > 0 ? Math.max(0, Math.min(count - 1, (index < 0 ? 0 : index) + delta)) : -1;
}
