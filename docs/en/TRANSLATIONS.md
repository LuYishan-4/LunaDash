# Interface translations

## Documentation languages

Technical documentation is separate from runtime UI catalogs. Canonical English guides live in `docs/en/`; their Traditional Chinese counterparts live in `docs/zh/` with matching filenames. `docs/README.md` is the language index, while the older root-level `docs/*.md` paths are compatibility forwarding pages. When documented behavior changes, update both language files.

The README translations under `docs/readme/` remain separate because they are complete translations of the repository landing page rather than technical-guide pages.


Keep C++, QML, identifiers, command arguments, paths, and protocol values in English.
Traditional Chinese interface text lives in `data/translations/zh_TW.json` and the
feature catalogs in `data/translations/zh_TW/`. New catalog files must be listed in
that directory; the native loader discovers catalogs by locale and installation
already includes them. One merged JSON dictionary feeds shell status and the
Slint Welcome, Package Manager and portal clients. English and unknown text falls
back to the source string.

The selected language is stored in the native desktop preferences. The loader
reads the older `LunaDash.conf` language key as a migration fallback; resetting
desktop preferences suppresses that legacy value while leaving the old file
untouched.

Use `shell.tr(source)` in QML. Slint clients use the shared JSON dictionary and
keep English source text as the fallback. Translate a template before substituting
variables, for example
`shell.tr("Switch to workspace %1").arg(number)`. Preserve every placeholder.
Do not translate serialized shortcut sequences, channel IDs, font-family values,
file names, application-provided notifications, or raw diagnostic output.

`StyledComboBox.translationContext` accepts the shell object. It translates the
visible label and popup rows without changing the model, `currentText`, or
`currentValue`. Keep source labels in model data instead of rebuilding translated
arrays whenever a shell status snapshot arrives. Settings search indexes both
source and translated aliases.

Run `python3 scripts/check-translations.py` and
`python3 scripts/test-translation-checker.py`. The coverage check scans literal
translation calls, common UI text properties, selected model-label tables, and
update-check errors. It checks JSON values, conflicting duplicates, cross-catalog
duplicates, and placeholder parity. It is not a semantic proof that arbitrary
runtime strings or external application content have translations. The original
catalog currently has a repeated identical `Use LunaDash default` key; this is
reported as a warning, while conflicting values are errors.

The translation-review workflow also parses all shell QML, builds the native
localization implementation without Qt, compares loaded dictionaries with source
catalogs, checks English fallback and locale preference migration, and runs a QML
dropdown regression for label/value separation and preserving scroll position
during a language change. These headless tests do not replace visual or
input-device testing in a session.

Translation catalogs are ordinary JSON files loaded from the source tree during
development and from `${datadir}/ludash/translations` after installation. Updating
an installed catalog does not require rebuilding LunaDash; restart the affected
client or compositor to reload its dictionary. Installed Arch sessions can be
rebuilt through `./scripts/install-session.sh`.
