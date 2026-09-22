# Extension/settings architecture audit

Audit input: `372646b898dec30b92b9c3ba2ef0af526d1bd528` on `fix/plugin`.
The source-contract workflow retains a revision, tracked-file inventory and
checksummed source archive so the audit can be reproduced.

## Removed after reference inspection

`compositor/animation/WindowAnimations.{cpp,hpp}` animated QQuickItems but had
no live callers. `compositor/window/WindowFrame.{cpp,hpp}` and
`ResizeGuide.{cpp,hpp}` were dormant Qt Quick adapters, kept alive only by the
`ludash-window-items` build target. All six files and that build target were
removed. The actual QML `WindowFrames` and wlroots `SceneAnimationBackend`
remain active and were not replaced with the obsolete adapters.

## Consolidated boundaries

- Module IDs, defaults, controls and limits moved from duplicate C++/QML forms
  into a shared data registry with common sections and explicit overrides.
- Extension and layout settings validation now uses the same host contract;
  the SDK counterpart runs shared fixtures. Effect configuration is exposed
  automatically alongside visual plugin configuration.
- Generic setting targets support scoped revision-checked updates, draft forms
  and filtering without knowing each plugin/module's setting names.
- Template layout actions describe required parameters and validate values
  before dispatch. The host supplies the trusted work area.
- Fixed the incomplete `WindowTemplate` type used by `Surface.cpp` and the
  plugin catalogue's iterators from two distinct temporary JSON arrays.

## Deliberately retained

Protocol constants, seat credentials, workspace/window identity, native library
lifetimes, buffer ownership, update installation, recovery UI and validation
limits are host responsibilities. They must not become unrestricted plugin
options simply because they are constants. Legacy launch/control aliases and
file-association migration code with real callers are retained. Concrete
built-in layout algorithms still implement their own geometry and focus policy;
removing those implementations would remove the default desktop.

This is a repository-wide source inventory plus a concrete cleanup of extension,
layout-action, module and settings boundaries, not proof that every executable
path is free of dead code or that every subsystem has a runtime plugin ABI.
SDK 2 remains a synchronous configuration/placement hook ABI; it does not permit
arbitrary replacement of wlroots resources or compositor renderer ownership.

## Regression checks

Run the source layout, architecture, render-boundary, QML action/style and
`tests/settings/test_schema.py` checks. CTest also builds/runs
`lunadash-settings`, covering shared fixtures, stale/read-only writes, module
host invariants and settings reaching the real native fade hook. The existing
QML runner includes `tst_settings_schema.qml` for generated controls. No test
was removed or relaxed to bypass a failed build. GPU/session integration is
still verified by the existing Wayland/OpenGL/startup workflows.
