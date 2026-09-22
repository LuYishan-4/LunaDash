#pragma once
#include <QDBusArgument>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace LunaDash {
class FilePickerDialog;
struct PortalFilterRule {
  uint type = 0;
  QString value;
};
struct PortalFileFilter {
  QString label;
  QList<PortalFilterRule> rules;
};
struct PortalChoiceValue {
  QString id;
  QString label;
};
struct PortalFileChoice {
  QString id;
  QString label;
  QList<PortalChoiceValue> values;
  QString selected;
};
QDBusArgument &operator<<(QDBusArgument &, const PortalFilterRule &);
const QDBusArgument &operator>>(const QDBusArgument &, PortalFilterRule &);
QDBusArgument &operator<<(QDBusArgument &, const PortalFileFilter &);
const QDBusArgument &operator>>(const QDBusArgument &, PortalFileFilter &);
QDBusArgument &operator<<(QDBusArgument &, const PortalChoiceValue &);
const QDBusArgument &operator>>(const QDBusArgument &, PortalChoiceValue &);
QDBusArgument &operator<<(QDBusArgument &, const PortalFileChoice &);
const QDBusArgument &operator>>(const QDBusArgument &, PortalFileChoice &);
void configureFilePicker(FilePickerDialog &picker, const QVariantMap &options);
QVariantMap filePickerResults(const FilePickerDialog &picker,
                              const QVariantMap &options);
QStringList portalSaveFileNames(const QVariantMap &options);
} // namespace LunaDash
Q_DECLARE_METATYPE(LunaDash::PortalFilterRule)
Q_DECLARE_METATYPE(LunaDash::PortalFileFilter)
Q_DECLARE_METATYPE(LunaDash::PortalChoiceValue)
Q_DECLARE_METATYPE(LunaDash::PortalFileChoice)
