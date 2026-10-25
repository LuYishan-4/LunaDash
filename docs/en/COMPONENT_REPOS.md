# Component repositories

LunaDash keeps the native desktop and Wayland compositor in this repository. The QML shell and Astro website have independent source repositories. Neither repository's source is present in a LunaDash checkout.

| Component | Repository | How LunaDash uses it |
| --- | --- | --- |
| Native desktop and compositor | [LunaDash](https://github.com/LuYishan-4/LunaDash) | C++20, C11 rendering core, Wayland integration and packaging |
| Desktop shell (Slint) | [LunaDash-Shell](https://github.com/LunaDashWM/LunaDash-Shell) | Slint source is installed with LunaDash; the selected commit is recorded in `packaging/shell-revision` |
| Shell runtime | [SlintLu](https://github.com/LunaDashWM/SlintLu) | Built from `packaging/slintlu-revision` and installed as `libexec/lunadash/slintlu` |
| Astro website | [LunaDash-docs](https://github.com/LunaDashWM/LunaDash-docs) | Website CI runs in that repository; LunaDash workflows clone it into a temporary runner directory |

The Shell commit selected by LunaDash changes only when `packaging/shell-revision` changes on the root `dev` branch. A new Shell commit does not update an existing LunaDash checkout or installation automatically. The website has its own history and is not pinned in LunaDash; each website check or Pages deployment fetches the current `main` branch of LunaDash-docs.

## Clone and install

The root checkout has no recursive submodules:

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
./scripts/install-session.sh
```

The normal install command fetches the pinned Shell source and installs it with LunaDash. To build from CMake directly, fetch the selected Shell revision first and pass its checkout path:

```sh
bash scripts/fetch-shell.sh ../LunaDash-Shell
cmake -S . -B build -G Ninja -DLUDASH_SHELL_SOURCE_DIR="$PWD/../LunaDash-Shell" \
  -DLUDASH_SLINTLU_SOURCE_DIR="$PWD/../SlintLu"
cmake --build build --parallel 2
```

LunaDash-Shell can also be installed on its own, without the integration repository:

```sh
git clone https://github.com/LunaDashWM/LunaDash-Shell.git
cd LunaDash-Shell
./install.sh
```

The standalone script installs Shell UI files to the user's data directory. `sudo ./install.sh --prefix /usr` installs them system-wide. It does not install LunaDash's native services; with `--install-slintlu` it builds SlintLu when it is missing. See [shell](SHELL.md).

## Change a component

Make QML changes in a LunaDash-Shell checkout. Commit and push them to its `main` branch, then update the Shell revision on LunaDash `dev` when that version should be integrated:

```sh
git -C ../LunaDash-Shell switch main
git -C ../LunaDash-Shell pull --ff-only origin main
git -C ../LunaDash-Shell rev-parse HEAD
```

Write the reviewed 40-character Shell commit to `packaging/shell-revision` in LunaDash. The Nix flake reads the same file. Commit that change on LunaDash `dev`; CI fetches the selected Shell commit and runs the native and QML suites.

Website changes belong in LunaDash-docs. Build it there with `npm ci` and `npm run build`, then commit and push to its `main` branch: LunaDash-docs deploys itself, and the root Pages workflow fetches that branch for its own deployment.

Keep compositor APIs, translations, native assets, CMake integration and technical documentation in LunaDash. The compositor uses shared LunaDash APIs, so it is not an independent CMake project.
