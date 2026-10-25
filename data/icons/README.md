# Interface icons

24×24 stroke icons (white strokes, tinted at runtime) for LunaDash's own Slint windows: the
portal file and screen-cast pickers, Welcome and the package manager. They come from SlintLu's
shared icon set (`res/icons` in https://github.com/LunaDashWM/SlintLu) and are compiled into
the executables, so no icon theme or symbol font is needed.

After adding or removing a file, run `scripts/gen-ui-icons.py` to regenerate
`src/desktop/theme/Icon.slint` (CI checks it).
