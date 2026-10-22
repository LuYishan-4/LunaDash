# Repository workflow

LunaDash keeps its desktop, Quickshell UI, compositor and Astro website in one Git repository. The source directories `src/compositor/`, `qml/` and `site/` are ordinary tracked directories in the `dev` branch; they are not submodules.

| Path | Responsibility |
| --- | --- |
| `src/compositor/` | Wayland compositor, built with the root CMake project and shared LunaDash APIs |
| `qml/` | Quickshell desktop UI, using the host's C++ services, translations and data |
| `site/` | Astro website, deployed by the root repository's Pages workflow |

The compositor and QML shell depend on APIs and assets elsewhere in LunaDash. Keeping them in the same repository lets one branch and commit update related code together. The former LunaDash-Shell, LunaDash-Compositor and LunaDash-docs repositories are historical component snapshots; normal development happens in this repository.

## Clone and build

Clone the development branch and build from the root:

```sh
git clone --branch dev https://github.com/LuYishan-4/LunaDash.git
cd LunaDash
cmake -S . -B build -G Ninja
cmake --build build --parallel 2
```

## Make a change

Edit any combination of files in one LunaDash checkout. Stage and commit them in the root repository, then push a branch and open a pull request targeting `dev`:

```sh
git switch -c feature/my-change
# edit files in src/compositor/, qml/, site/ or other project paths
git add src/compositor qml site
git commit -m "Describe the change"
git push -u origin feature/my-change
```

There is no component commit or submodule pointer to update. The website has its own `package.json` and can be checked from `site/`, while desktop and Wayland integration is built from the root project.
