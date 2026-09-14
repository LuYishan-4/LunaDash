#pragma once

#include <QJsonObject>
#include <QString>

class QKeyEvent;

namespace LuDash {

class ShortcutSettings {
public:
  ShortcutSettings();

  QJsonObject snapshot() const;
  QString actionFor(const QKeyEvent &event) const;
  bool apply(const QJsonObject &changes, QString *error);
  void reset();

private:
  QJsonObject bindings_;
};

} // namespace LuDash
