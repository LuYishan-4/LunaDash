#include <LuDash/shortcut_settings/ShortcutSettings.h>

#include <QKeyEvent>
#include <QKeySequence>
#include <QSet>
#include <QSettings>

namespace LuDash {
namespace {

QJsonObject defaults() {
  QJsonObject result;
  result.insert("focusLeft", "Meta+H");
  result.insert("focusRight", "Meta+L");
  result.insert("focusUp", "Meta+K");
  result.insert("focusDown", "Meta+J");
  result.insert("groupLeft", "Meta+Shift+H");
  result.insert("groupRight", "Meta+Shift+L");
  result.insert("reorderLeft", "Meta+Ctrl+H");
  result.insert("reorderRight", "Meta+Ctrl+L");
  result.insert("expelWindow", "Meta+Shift+E");
  result.insert("centerColumn", "Meta+Shift+C");
  result.insert("widenColumn", "Meta+=");
  result.insert("narrowColumn", QStringLiteral("Meta+") + QChar('-'));
  result.insert("maximizeWindow", "Meta+F");
  result.insert("closeWindow", "Meta+C");
  result.insert("minimizeWindow", "Meta+M");
  result.insert("closeWindowAlternate", "Meta+Q");
  result.insert("toggleFloating", "Meta+Space");
  result.insert("launchTerminal", "Meta+Return");
  result.insert("launchFiles", "Meta+E");
  result.insert("launchLauncher", "Meta+D");
  result.insert("screenshot", "Alt+Shift+F5");
  for (int workspace = 1; workspace <= 9; ++workspace) {
    result.insert(QString("workspace%1").arg(workspace),
                  QString("Meta+%1").arg(workspace));
    result.insert(QString("moveToWorkspace%1").arg(workspace),
                  QString("Meta+Shift+%1").arg(workspace));
  }
  return result;
}

QString normalizedSequence(const QString &text) {
  if (text == "Disabled")
    return text;
  const auto sequence =
      QKeySequence::fromString(text, QKeySequence::PortableText);
  if (sequence.count() != 1)
    return {};
  const auto combination = sequence[0];
  const auto modifiers = combination.keyboardModifiers();
  // A global shortcut claims one combination that keeps a text key reachable,
  // so it must use Meta or Alt. Ctrl alone stays available to applications.
  if (!modifiers.testFlag(Qt::MetaModifier) &&
      !modifiers.testFlag(Qt::AltModifier))
    return {};
  if (combination.key() == Qt::Key_unknown ||
      combination.key() == Qt::Key_Meta ||
      combination.key() == Qt::Key_Control ||
      combination.key() == Qt::Key_Shift || combination.key() == Qt::Key_Alt)
    return {};
  return QKeySequence(combination).toString(QKeySequence::PortableText);
}

} // namespace

ShortcutSettings::ShortcutSettings() : bindings_(defaults()) {
  QSettings settings;
  const auto configured =
      QJsonObject::fromVariantMap(settings.value("shortcuts/bindings").toMap());
  QString error;
  if (!configured.isEmpty())
    apply(configured, &error);
}

QJsonObject ShortcutSettings::snapshot() const { return bindings_; }

QString ShortcutSettings::actionFor(const QKeyEvent &event) const {
  const auto pressed =
      QKeySequence(event.keyCombination()).toString(QKeySequence::PortableText);
  for (auto it = bindings_.begin(); it != bindings_.end(); ++it)
    if (it.value().toString() != "Disabled" && it.value().toString() == pressed)
      return it.key();
  return {};
}

bool ShortcutSettings::apply(const QJsonObject &changes, QString *error) {
  auto candidate = bindings_;
  const auto known = defaults();
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (!known.contains(it.key()) || !it.value().isString()) {
      if (error)
        *error = "Unknown shortcut action: " + it.key();
      return false;
    }
    const auto sequence = normalizedSequence(it.value().toString());
    if (sequence.isEmpty()) {
      if (error)
        *error = "Shortcut must be one Meta or Alt key combination.";
      return false;
    }
    candidate.insert(it.key(), sequence);
  }
  QSet<QString> used;
  for (auto it = candidate.begin(); it != candidate.end(); ++it) {
    const auto sequence = it.value().toString();
    if (sequence == "Disabled")
      continue;
    if (used.contains(sequence)) {
      if (error)
        *error = "Shortcut is already assigned: " + sequence;
      return false;
    }
    used.insert(sequence);
  }
  bindings_ = candidate;
  QSettings settings;
  settings.setValue("shortcuts/bindings", bindings_.toVariantMap());
  settings.sync();
  if (settings.status() != QSettings::NoError) {
    if (error)
      *error = "Could not save shortcut settings.";
    return false;
  }
  return true;
}

void ShortcutSettings::reset() {
  bindings_ = defaults();
  QSettings settings;
  settings.remove("shortcuts/bindings");
  settings.sync();
}

} // namespace LuDash
