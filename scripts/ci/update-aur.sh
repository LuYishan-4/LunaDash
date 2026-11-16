#!/usr/bin/env bash
# Update the slintlu, lunadash-shell and ludash AUR packages for a LunaDash release tag.
#
# Usage: scripts/ci/update-aur.sh <tag> [--push]
#
# The PKGBUILD templates come from packaging/aur/ at the tag. ludash is pinned to the tag,
# lunadash-shell and slintlu to packaging/shell-revision and packaging/slintlu-revision. Without
# --push the resulting AUR commits are only shown. Needs git, makepkg and updpkgsums
# (pacman-contrib); makepkg does not run as root. Nothing is compiled.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
tag=${1:?usage: update-aur.sh <tag> [--push]}
push=false
case ${2:-} in
    --push) push=true ;;
    '') ;;
    *) echo "Unknown option: $2" >&2; exit 2 ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/lunadash-aur.XXXXXXXX")
trap 'rm -rf -- "$work"' EXIT

commit=$(git -C "$project_dir" rev-parse --verify "$tag^{commit}")
show() { git -C "$project_dir" show "$commit:$1"; }

# pkgver may not contain '-', ':' or '/'.
release_version=${tag#v}
release_version=${release_version//-/_}
[[ $release_version =~ ^[A-Za-z0-9._+]+$ ]] || { echo "Tag $tag is not a usable pkgver" >&2; exit 1; }

# Prints "<count> <short hash>" for a revision of a component repository.
component_revision() {
    local url=$1 revision=$2 dir
    dir=$work/$(basename "$url" .git)
    [[ -d $dir ]] || git clone --quiet "$url" "$dir"
    printf '%s %s\n' "$(git -C "$dir" rev-list --count "$revision")" \
        "$(git -C "$dir" rev-parse --short=7 "$revision")"
}

srcinfo_value() { sed -n "s/^\t$2 = //p" "$1/.SRCINFO" 2>/dev/null | head -n 1; }

# update_package <name> <commit> <pkgver>
update_package() {
    local name=$1 pin=$2 version=$3 repo=$work/aur-$1 old_version old_release release
    git clone --quiet "https://aur.archlinux.org/$name.git" "$repo"
    git -C "$repo" remote set-url --push origin "ssh://aur@aur.archlinux.org/$name.git"
    old_version=$(srcinfo_value "$repo" pkgver)
    old_release=$(srcinfo_value "$repo" pkgrel)
    if [[ -n $old_version ]] && (($(vercmp "$version" "$old_version") < 0)); then
        echo "$name: $version is older than the published $old_version; not downgrading" >&2
        exit 1
    fi

    show "packaging/aur/$name/PKGBUILD" > "$repo/PKGBUILD.new"
    write_pkgbuild() {
        sed -e "s/^_commit=.*/_commit=$pin/" -e "s/^pkgver=.*/pkgver=$version/" \
            -e "s/^pkgrel=.*/pkgrel=$1/" "$repo/PKGBUILD.new" > "$repo/PKGBUILD"
        (cd "$repo" && updpkgsums >/dev/null)
    }
    # A new version starts at pkgrel 1; a changed template for the same version bumps pkgrel.
    release=1
    if [[ $old_version == "$version" ]]; then
        release=$old_release
        write_pkgbuild "$release"
        git -C "$repo" diff --quiet -- PKGBUILD || release=$((old_release + 1))
    fi
    write_pkgbuild "$release"
    rm -f "$repo/PKGBUILD.new"
    (cd "$repo" && makepkg --printsrcinfo > .SRCINFO)
    printf '*\n!PKGBUILD\n!.SRCINFO\n!.gitignore\n' > "$repo/.gitignore"

    git -C "$repo" add -f PKGBUILD .SRCINFO .gitignore
    if git -C "$repo" diff --cached --quiet; then
        echo "$name: already at $version-$release"
        return
    fi
    git -C "$repo" commit --quiet -m "Update to $version-$release"
    echo "$name: $old_version-$old_release -> $version-$release"
    if $push; then
        git -C "$repo" push --quiet origin HEAD:master
    else
        git -C "$repo" --no-pager show --stat --format='%s' HEAD
    fi
}

slintlu_revision=$(show packaging/slintlu-revision)
revision_info=$(component_revision https://github.com/LunaDashWM/SlintLu.git "$slintlu_revision")
read -r count short <<< "$revision_info"
slintlu_version=$(git -C "$work/SlintLu" show "$slintlu_revision:Cargo.toml" |
    sed -n 's/^version = "\(.*\)"$/\1/p' | head -n 1)
update_package slintlu "$slintlu_revision" "${slintlu_version:-0}.r$count.g$short"

shell_revision=$(show packaging/shell-revision)
revision_info=$(component_revision https://github.com/LunaDashWM/LunaDash-Shell.git "$shell_revision")
read -r count short <<< "$revision_info"
update_package lunadash-shell "$shell_revision" "0.r$count.g$short"

update_package ludash "$commit" "$release_version"
