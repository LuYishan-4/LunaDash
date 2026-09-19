#pragma once

#include <QJsonObject>
#include <QString>
#include <cstdint>
#include <xkbcommon/xkbcommon.h>

namespace LunaDash {

enum ShortcutModifier : uint32_t {
  ShortcutMeta = 1u << 0,
  ShortcutControl = 1u << 1,
  ShortcutAlt = 1u << 2,
  ShortcutShift = 1u << 3,
};

class ShortcutSettings {
public:
  ShortcutSettings();

  QJsonObject snapshot() const;
  QString actionFor(xkb_keysym_t keysym, uint32_t modifiers) const;
  bool apply(const QJsonObject &changes, QString *error);
  void reset();

private:
  QJsonObject bindings_;
};

} // namespace LunaDash
