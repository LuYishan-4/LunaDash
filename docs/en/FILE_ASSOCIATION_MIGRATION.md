# Migrating the early file-association implementation

The first modular Files launch imports recognized entries from the earlier
QSettings `fileAssociations/ext_*` and `fileAssociations/mime_*` groups into
`$XDG_CONFIG_HOME/LunaDash/file-associations.json`. Existing JSON rules win. The
old keys are retained as a backup. A migration-version marker prevents deleted
JSON rules from being imported again. Missing or unrecognized desktop entries
are not launched; Files reports them and prompts for a new handler on next use.

The `__system__` rule follows the current system MIME handler instead of pinning
its executable. It is also available as **Use the current system default** in
the chooser when the system has an installed handler. If that handler later
vanishes, the chooser is shown again.

Right-click **Set default application for this file type...** saves a rule
without launching a document. **Reset default application for this file type**
removes only the Files rule. Sidebar Open, Open terminal here and Copy paths
remain available; sidebar Open in a new window is also supported. Ctrl+R and F5
refresh, and Ctrl+H toggles hidden entries.

Earlier builds also wrote system MIME defaults when saving extension rules.
Migration does not undo those system settings. New changes affect only Files
unless the user explicitly checks the separate system-default option.
