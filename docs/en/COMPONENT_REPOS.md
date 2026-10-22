# Component repositories

LunaDash keeps the website, Quickshell shell and compositor source in separate GitHub repositories. The integration repository's `dev` branch pins exact component commits with Git submodules:

| Integration path | Repository | Current build role |
| --- | --- | --- |
| `site/` | [LunaDash-docs](https://github.com/LunaDash-DE/LunaDash-docs) | Standalone Astro website; deployed by the integration workflow |
| `qml/` | [LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) | QML assets loaded by the integrated Quickshell runtime |
| `src/compositor/` | [LunaDash-Compositor](https://github.com/LunaDash-DE/LunaDash-Compositor) | Compositor source compiled by the integration CMake project |

The root LunaDash repository remains the integration project. Its CMake files own target definitions and combine the compositor with shared `src/config`, `src/core`, `src/desktop` and `src/shell` APIs. The compositor repository therefore is not yet a standalone CMake build. Shell QML also relies on the host's C++ services, translations and data assets. A pinned component commit is a version boundary, not an automatic update or a promise of an independent runtime.

## Clone and build

Initialize the exact versions selected by `dev` when cloning:

```sh
git clone --recurse-submodules --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
git submodule status
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

An existing checkout can initialize or restore the recorded component versions with:

```sh
git submodule update --init --recursive
```

Do not use `git submodule update --remote` for a normal build; it moves the local checkout to newer component commits without changing the integration pin.

## Change a component and integrate it

Make the change in the component repository, commit and push that repository, then review its commit in the integration checkout. For example, after a QML change has been committed to `LunaDash-Shell`:

```sh
git -C qml fetch origin main
git -C qml checkout --detach <reviewed-component-sha>
git diff --submodule=log -- qml
git add qml
git diff --cached --submodule=log
git commit -m "Update pinned QML shell"
```

The component URL must be pushed before the integration commit so fresh clones can fetch it. Commit the integration pointer on `dev`; do not move `main` as part of ordinary component work. Website, QML and compositor source edits each need their matching component commit and one integration pin update. CI, the website deploy workflow, Nix and the Arch source packager initialize the pinned submodules.

For a quick update to every component's configured `main` branch, inspect the fetched commit IDs first:

```sh
git submodule update --remote site qml src/compositor
git diff --submodule=log
git add site qml src/compositor
git commit -m "Update pinned component revisions"
```

This command changes only the local worktree and the integration repository's staged pointers. It does not commit or push changes in any component repository.

## Current extraction boundary

The website has its own Node package and can be checked and built in its repository. QML is versioned independently, while application builds and behavior tests run in LunaDash because its host APIs and test assets live there. The compositor sources have a stronger dependency on shared integration domains, so all production CMake targets and Wayland runtime tests remain in LunaDash. Moving those APIs into a stable public core would be required before advertising the compositor as independently buildable.
