# Files and file associations

Files uses the existing native Qt interface and shared desktop theme. Layout,
interaction and association logic are separated into `src/desktop/filemanager/` and `src/desktop/fileoperations/`, with separate
interfaces for actions, association UI and GIO integration.

## First use and opening documents

The first Files window asks whether each previously unseen file type should show
an application chooser. This is on by default. Closing the first-use dialog
without accepting does not mark setup as completed. The preference can always be
changed with the **File associations** toolbar button or background context menu.

Double-click a document, or select files of one type and choose **Open with…**.
The chooser lists installed desktop applications, prioritizes recommended and
system-default handlers, and includes search and **Show all installed applications**.
Leave **Always use this application** unchecked to open once without saving.
An uninstalled saved handler causes a new prompt rather than a silent failure.
Directories stay in Files; selected desktop entries and binaries are never passed
to a shell as executable commands. Opening a document passes it as an argument to
an explicitly selected installed handler. As with all desktop launchers, only
install applications and desktop entries you trust.

Extension rules are stored atomically under
`$XDG_CONFIG_HOME/LunaDash/file-associations.json` (normally
`~/.config/LunaDash/file-associations.json`). Every field has a GUI control;
manual JSON editing is not necessary. Invalid files are reported and not
silently replaced. A lock prevents concurrent windows from losing each other's
edits. A rule looks like:

```json
{
  "version": 1,
  "initialized": true,
  "askOnFirstOpen": true,
  "associations": {
    "ext:txt": {"desktopId": "org.example.Editor.desktop", "mimeType": "text/plain"}
  }
}
```

Extensions are case-insensitive. Explicit compound extensions use the longest
matching rule. Files without an extension use a `mime:<type>` key. An extension
rule for `.txt` does not also change `.log`, even when they share a MIME type.
The optional, unchecked **Also make it the system default** control uses GIO's
standard MIME association API and affects other applications and extensions with
that MIME type. Removing a Files rule does not reset a system MIME association.
GIO also handles desktop-entry `Exec` field codes, URI arguments, terminal and
D-Bus activation; Files does not concatenate filenames into a shell command.

## File interactions

Both icon and details views share a selection. Right-clicking a selected item
preserves multi-selection; right-clicking an unselected item targets its row;
right-clicking empty space targets the current directory. Keyboard context-menu
requests also work. The menu offers opening, choosing applications, new Files
windows for folders, cut/copy/paste, renaming, Trash with confirmation, file and
folder creation, a terminal in the target directory, copying paths, properties,
sorting, hidden files and refresh.

Ctrl+C/X/V/A, Ctrl+Shift+C, F2, Delete, Enter, Alt+Enter, Ctrl+N,
Ctrl+Shift+N and Ctrl+Alt+T work in the file views. Text fields retain their own
clipboard shortcuts. Alt+Left/Right/Up, Ctrl+L and F5 provide navigation/refresh.
Icon/details mode, hidden files and sorting are remembered. The system clipboard
uses local file URIs and both KDE and GNOME cut markers, rather than an isolated
per-window clipboard. Successful cut/paste clears only the clipboard snapshot
that was actually moved.

Drag local files onto a directory to copy; hold Shift to request a move. Remote
URL drops are not treated as local files. Copying supports directories, hidden
entries and symbolic links without following links. A temporary destination and
Linux `renameat2(RENAME_NOREPLACE)` prevent an incomplete copy from replacing an
existing item. Existing targets, duplicate target names, copying into oneself,
and selecting a directory together with its descendants are rejected. Special
files such as FIFOs and device nodes are not copied.

File operations are asynchronous; navigation does not change a running
operation's captured destination. There is no global undo/transaction spanning
multiple entries: an error explicitly reports that earlier entries may have
completed. A move uses an atomic rename on the same filesystem and falls back
to an atomic staged copy followed by removing the source when the destination is
on another filesystem; if the source cannot be removed, the failure is reported
instead of leaving a silent duplicate. Copying preserves basic permissions and modification
times, not ownership, ACLs or extended attributes. Permanent deletion, archive
editing, recursive directory-size calculation and remote filesystem mounting are
not added by this change.

## Build and regression tests

Arch packages include `glib2`, `shared-mime-info` and the `glib2-devel` build tools.
Ubuntu builders need `libglib2.0-dev`. All complete-build CI jobs include it.

```sh
cmake -S . -B build -DLUDASH_BUILD_FILES_TESTS=ON
cmake --build build --parallel 2
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -R '^lunadash-files$'
```

Tests isolate XDG configuration and desktop entries in a temporary directory.
They cover extension rules, corrupt-config preservation, first-use prompts,
selection/context behavior, the shared clipboard, directories and symlinks,
no-overwrite and recursion checks, special-file rejection, same-filesystem moves,
and desktop-launch arguments containing quotes and shell metacharacters.

Built-in plugin and portal file chooser windows use the shell palette, rounded controls, and the configured font and accent. Open windows follow appearance changes. Their Qt Widgets surfaces remain opaque internally; the compositor's window-opacity and frosted-glass controls can reveal a blurred background behind ordinary application windows. QML motion effects are not reproduced inside Qt Widgets. External applications that bypass the LunaDash portal use their own file chooser theme.
