#!/usr/bin/env bash
# Fetches one commit of a GitHub repository into a new directory. GitHub is tried
# first, then the gh-proxy.org mirror; LUNADASH_GIT_MIRRORS replaces that mirror
# list (space-separated URL prefixes, empty for GitHub only). The commit id is
# checked after the fetch, so a mirror cannot substitute other content.
set -euo pipefail

name=${1:?Usage: fetch-pinned.sh NAME URL DESTINATION REVISION}
repository=${2:?Usage: fetch-pinned.sh NAME URL DESTINATION REVISION}
destination=${3:?Usage: fetch-pinned.sh NAME URL DESTINATION REVISION}
revision=${4:?Usage: fetch-pinned.sh NAME URL DESTINATION REVISION}

[[ $revision =~ ^[0-9a-f]{40}$ ]] || {
    printf 'Invalid %s revision: %s\n' "$name" "$revision" >&2
    exit 2
}
[[ ! -e $destination ]] || {
    printf 'Destination already exists: %s\n' "$destination" >&2
    exit 2
}

read -ra mirrors <<< "${LUNADASH_GIT_MIRRORS-https://gh-proxy.org/}"
sources=("$repository")
for prefix in "${mirrors[@]}"; do sources+=("$prefix$repository"); done

git -c maintenance.auto=false init --quiet "$destination"
git -C "$destination" remote add origin "$repository"
fetched=false
for url in "${sources[@]}"; do
    if env GIT_TERMINAL_PROMPT=0 git -c http.lowSpeedLimit=1000 -c http.lowSpeedTime=20 \
        -C "$destination" fetch --quiet --no-tags --depth=1 "$url" "$revision"; then
        fetched=true
        [[ $url == "$repository" ]] || printf 'Fetched %s through %s\n' "$name" "$url"
        break
    fi
    printf 'Could not fetch %s from %s\n' "$name" "$url" >&2
done
$fetched || {
    printf 'Fetching %s %s failed from every source.\n' "$name" "$revision" >&2
    exit 1
}
git -C "$destination" checkout --quiet --detach FETCH_HEAD
actual=$(git -C "$destination" rev-parse HEAD)
[[ $actual == "$revision" ]] || {
    printf 'Downloaded %s %s, expected %s.\n' "$name" "$actual" "$revision" >&2
    exit 1
}
printf 'Fetched %s %s\n' "$name" "$actual"
