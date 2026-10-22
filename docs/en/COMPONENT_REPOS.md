# Source repository layout

The LunaDash root repository keeps the compositor, desktop services and build integration. The Quickshell UI and Astro website have their own organization repositories and are pinned into the root checkout as Git submodules.

| Path | Source | Role |
| --- | --- | --- |
| `src/compositor/` | LunaDash root repository | Wayland compositor built with shared LunaDash APIs |
| `qml/` | [LunaDash-Shell](https://github.com/LunaDash-DE/LunaDash-Shell) | Quickshell UI loaded by the desktop session |
| `site/` | [LunaDash-docs](https://github.com/LunaDash-DE/LunaDash-docs) | Astro website deployed by the root Pages workflow |

Each root commit records the exact QML and website commits at `qml/` and `site/`. A new commit in either organization repository does not change the version selected by LunaDash until the root submodule pointer is updated. C++ host APIs, translations, assets and CMake integration remain in the root repository.

## Clone and build

Fetch the root repository and its pinned components together:

```sh
git clone --recurse-submodules --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
git submodule status
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

For an existing checkout, initialize or restore the pinned sources with:

```sh
git submodule update --init --recursive
```

## Change the shell or website

Commit and push a QML change in LunaDash-Shell or a website change in LunaDash-docs first. Then update the root repository's pinned revisions and commit those pointers on a branch targeting `dev`:

```sh
git -C qml switch main
git -C qml pull --ff-only origin main
git -C site switch main
git -C site pull --ff-only origin main
git submodule update --remote qml site
git diff --submodule=log -- qml site
git add qml site
git commit -m "Update pinned shell and website"
git push
```

If only one component changed, stage only its path. The root commit contains the new submodule pointer, not another copy of the component files. Keep related root API, CMake, translation or documentation changes in the root repository. The compositor is not an independent CMake project because it consumes shared APIs from LunaDash.
