# Interface translations

Keep C++, QML, identifiers, command arguments, paths, and protocol values in English.
Traditional Chinese interface text lives in `data/translations/zh_TW.json` and the
feature catalogs in `data/translations/zh_TW/`. New catalog files must be listed in
`cmake/LunaDashTranslations.cmake`; installation already includes this directory.
The same merged dictionary feeds native Qt translations and shell status snapshots.
English and unknown application text fall back to the source string.

Use `shell.tr(source)` in QML and `LunaDash::translate(source)` in native UI code.
Translate a template before substituting variables, for example
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

The translation-review workflow also parses all shell QML, builds the actual
localization implementation against Qt Core, compares the embedded catalogs with
the source catalogs, tests English fallback, and runs a QML dropdown regression
for label/value separation and preserving scroll position during a language change.
These headless tests do not replace visual or input-device testing in a session.

Translations are compiled into Qt resources. After pulling changes, rebuild with
`cmake --build build` and restart the compositor; reloading QML alone cannot update
the compiled dictionary. Installed Arch sessions can be rebuilt through
`./scripts/install-session.sh`.
