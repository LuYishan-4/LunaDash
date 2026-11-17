# Desktop interaction and plugin templates

## Keyboard and focus

Workspaces follow the windows: there is always exactly one empty workspace after
the last occupied one, a window on that empty workspace adds the next one, and an
empty workspace between occupied ones disappears once you leave it. Switching
slides the whole workspace sideways (Reduced motion turns the slide off).

`Super+Tab` or a tap of Super alone opens the workspace overview, like niri's. The compositor
zooms the screen out to half size: every workspace is stacked vertically around the current
one and every window is drawn live where the layout places it, over the dimmed wallpaper;
the shell's bar stays on top. Click a window to switch to it (and close the overview), or a
free spot to switch to that workspace. Drag a window onto another window to swap them,
moving it to that workspace first when needed, or onto a free spot of another workspace to
move it there; the overview stays open. Middle-click closes a window. Scroll switches
workspaces. Arrow keys or H/J/K/L move the focus (left/right between columns, up/down between
workspaces); with Ctrl they move the focused column, or the focused window to the workspace
above/below. These overview keys are registered actions and can be customized alongside other
shortcuts; their unmodified bindings are active only while the overview is open. Esc, Enter or
Super closes it, and the compositor's other shortcuts keep working.
`lunadashctl overview toggle|open|close` drives it from scripts or a shell button. The
previews have square corners and no shadows yet.

`Alt+Tab` / `Alt+Shift+Tab` opens the window
switcher: every mapped application window in the taskbar, the current workspace
first, including minimized windows. Release Alt to focus and restore the selected
window, switching to its workspace. The shell draws both as a box in the middle of
the screen; clicking an entry switches to it, and the window button beside the
panel's workspace dots opens the same box (right-click: workspaces).
Neither shortcut invokes the taskbar's maximize toggle. An already maximized
workspace continues to use the existing focus/maximization policy.

Both selectors accept arrows, wheel movement, pointer selection, Enter and Escape.
Escape cancels. Switching workspace by another route cancels a pending window
selection; destroying/unmapping a candidate removes it without invalid indices.
The shortcuts, including Shift variants, are reserved; legacy conflicting custom
bindings are normalized to Disabled instead of silently intercepting the selector.

Window previews use the compositor's bounded 320×200 snapshot/readback path, not
continuous video capture. Missing/unreadable thumbnails retain an icon and title.

## Consistent popups

Taskbar and dock popup buttons toggle: press again to close. Control-center tabs
close when their already-open tab is pressed again, or switch tabs otherwise.
Only one transient surface is active at once. A left press on the desktop or an
ordinary application dismisses transient surfaces without swallowing the click.
Layer-shell controls and their nested menus are not treated as outside clicks.
The compositor publishes this event through its owner-only interaction channel;
there is no permanent full-screen input blocker.

The launcher immediately grants its search field keyboard focus after either mouse
or keyboard activation. Typing from the results list returns input to search.
Closing releases keyboard ownership. Built-in passive hover tooltip popups are
removed; button names remain available to accessibility clients.

## Replaceable selectors

`window-switcher` is the Alt+Tab target; `workspace-switcher` is the Super+Tab target.
Both supply `context.interaction`, `accent`, `background`, `foreground`, `muted`
and `fontFamily`. `interaction.scope` is `windows` or `workspaces`; the applicable
array is `interaction.windows` or `interaction.workspaces`, with a zero-based
`index`. Window IDs are compositor IDs; workspace IDs in the grid are 1–10.

Use `shell.command("switch-window", id)` to highlight, `switch-accept` to confirm,
and `switch-cancel` to cancel. The host owns key releases, selection lifetime and
the overlay. A replacement root is an Item, not another PanelWindow.
Invalid/unavailable replacements retain built-in content.

## Full-screen plugin activation transition

Every effective plugin enablement change produces a transition; status polling,
unchanged saves and initial discovery do not. Multiple targets in one package
produce one event, and conflicting package changes queue rather than cancel each
other. The layer covers all connected outputs using a single progress clock.
It has an empty input region and no keyboard focus, so it cannot consume clicks or
block typing. Reduced motion/disabled animations cancel the queue immediately.

The `plugin-transition` target receives `context.progress` (0–1), `change`
(`id`, `name`, `enabled`, `targets`, `previousTargets`), `duration` and desktop
colors/font. The host owns timing even when its animation plugin is toggled.
Built-in duration is configurable from 100–1600 ms (default 420). The
transition visual follows accepted state changes; it is not a transaction barrier
that delays native plugin unload or makes native code safe.

## Retired custom module QML

The old Shell modules custom-code controls, loose-file loader, template installer
and `module-code-trust` / `module-template` IPC commands are removed. Existing
`custom` JSON fields are accepted only for migration and discarded during
normalization. No legacy QML is evaluated, and existing user files are not deleted.
Safe module `enabled`, `style` and `config` settings retain their existing schema.
All new visual code uses metadata-based SDK 2 plugins. Traditional Chinese plugin
terminology is standardized separately from ordinary module/layout settings.

## Validation

Run the source-layout and shell action checks. Native Wayland keyboard delivery,
fast repeated clicks and screenshots of the actual shell still require a real
session.
