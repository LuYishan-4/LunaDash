# Shared settings API and schema-generated controls

The compositor exports **one configuration description API** for native effects,
Quickshell/OpenGL plugins, built-in features, window layouts and shell modules.
An `effect` plugin does not need a QML settings page. Its validated SDK `settings`
manifest is discovered automatically, including while that plugin is disabled.
Saving options never implicitly enables a plugin or trusts custom code.

## Discover, edit, apply

`lunadashctl settings-describe` returns `{version: 1, targets: [...]}`. The same
object is published as `status.settingsApi`, consumed by the shell's existing
state channel. Each target has `id`, `name`, `type`, `category`, `schema`,
`values` and `revision`.

Stable target forms are `plugin:<plugin-id>`, `builtin:<feature-id>`,
`layout:<active-template>` and `module:<module-id>:<section>`. Module sections
are `module`, `style`, `config` and `custom`. The layout template has its own
options; the legacy built-in gap field is not shown as a second editor.

```sh
lunadashctl settings-describe
# Copy this target's current revision from the description, not a made-up value.
lunadashctl settings-update '{"target":"plugin:org.ludash.fade","revision":"<current revision>","changes":{"duration":300,"softFocus":false}}'
```

A write checks the target, schema, value types/ranges, read-only fields and
revision before merging only that target's options into the existing document.
Unknown fields and obsolete revisions return errors. Successful calls return
updated shell state and `settingsTarget`. Disk operations use the existing
atomic configuration writers. Revision checks prevent stale form overwrites;
they are not an interprocess locking protocol for arbitrary external writers.

The settings pages use Apply/Reload/Defaults. Edited values remain a draft until
Apply; errors do not discard the draft. If the target changed since it was
loaded, Reload is required. Plugin enable/composition controls remain explicit.

## Standard control contract

A setting declares a data `type`, `default`, and optional `label`, `description`,
`order`, `minimum`, `maximum`, `step`, `enum`, `readOnly`, and `control`.

| Control | Value | Existing shell components |
| --- | --- | --- |
| `toggle` | boolean | `SoftSwitch`, Yes/No |
| `select` | a member of `enum` | `StyledComboBox`; stored values are not translated |
| `number` | integer or number | `SoftField` with decrease/increase buttons |
| `slider` | bounded integer or number | `SettingsSlider` / `SoftSlider` |

The UI infers a control when omitted: enums become dropdowns, booleans become
switches, bounded numbers become sliders, other numbers get numeric editors.
An explicit `control` can choose numeric entry rather than a slider. `step` is
an interaction increment, not a requirement that typed values lie on a grid.
Fractional values remain fractional. JavaScript-unsafe integers, non-finite
numbers, wrong types, duplicate enum entries and incompatible controls fail
validation in both the SDK and host.

```json
{
  "softFocus": {"type":"boolean","default":true,"control":"toggle"},
  "easing": {"type":"string","default":"linear","enum":["linear","outCubic"],"control":"select"},
  "duration": {"type":"integer","default":260,"minimum":0,"maximum":600,"step":10,"control":"number"},
  "exitScale": {"type":"number","default":0.9,"minimum":0.5,"maximum":1,"step":0.01,"control":"slider"}
}
```

Existing free-form strings, paths and colors keep the `text` compatibility
editor rather than being coerced into an unrelated control. Existing bounded
string collections use `select` with multiple switches. These forms retain
host path/pattern validation. `specialValues` supports legacy numeric sentinels
such as zero meaning automatic size; those fields use numeric entry, not a
misleading slider spanning a forbidden interval.

`data/plugins/fade` demonstrates all four controls and consumes each option in
its native hook. SDK ABI 2 and manifest schemaVersion 2 are unchanged; rebuild
packages when their manifest changes. Schema checking lives in
`core/settings/SettingsSchema.hpp` and the installed SDK `SettingsSchema.py`;
both run the shared `tests/settings/contract.json` fixtures.

## Modular settings

Settings → Shell modules now filters targets by implementation type, category,
search text and whether configurable options exist. The same form works for
`effect`, `quickshell`, `opengl`, `module`, `layout` and `builtin` targets. All
controls reuse the shell's Theme and existing QML widgets.

`data/modules/registry.json` owns module identities, shared section schemas,
per-module overrides, defaults, ranges, labels and template availability. The
host resolves common sections before exporting descriptors. QML no longer has
a parallel list of module IDs, numeric bounds or hand-built module forms.
The existing module JSON format remains readable. Settings/setup/feedback
recovery modules cannot be disabled. Panel cross-field size rules, canonical
custom-QML paths, file-size limits, symlink protection and explicit code trust
remain host responsibilities; schema hints cannot bypass them.

## Layout action boundary

Layout-specific actions stay behind `performAction`, not new pure virtual
methods for every direction/group operation. Template action descriptors now
include parameter schemas and required fields. Invalid/missing/fractional IDs,
invalid directions and unexpected parameters are rejected before dispatch.
`window-layout-action` supplies the compositor work area itself; callers must
not provide an `area`. Resource/protocol operations are not exposed as arbitrary
settings or plugin callbacks.
