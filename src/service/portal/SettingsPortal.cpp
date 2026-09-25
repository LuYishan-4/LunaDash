#include "service/portal/SettingsPortal.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "service/portal/FileChooserPortal.hpp"
#include <QDBusMetaType>
#include <QTimer>

namespace LunaDash {
SettingsPortal::SettingsPortal(QObject *parent) : QDBusAbstractAdaptor(parent) {
  qDBusRegisterMetaType<PortalSettingsMap>();
  refresh();
  auto *timer = new QTimer(this);
  connect(timer, &QTimer::timeout, this, &SettingsPortal::refresh);
  timer->start(1000);
}
uint SettingsPortal::version() const { return 2; }
void SettingsPortal::refresh() {
  const auto preferences = desktopPreferences();
  const QVariantMap next{
      {"color-scheme", uint(appearanceIsDark(preferences) ? 1 : 2)},
      {"contrast", uint(0)},
      {"reduced-motion",
       uint(preferences.value("animations").toBool() ? 0 : 1)}};
  for (auto it = next.begin(); it != next.end(); ++it)
    if (values_.value(it.key()) != it.value())
      emit SettingChanged("org.freedesktop.appearance", it.key(),
                          QDBusVariant(it.value()));
  values_ = next;
}
PortalSettingsMap SettingsPortal::ReadAll(const QStringList &namespaces) {
  refresh();
  for (const auto &filter : namespaces) {
    if (filter == "org.freedesktop.appearance" ||
        (filter.endsWith('*') && QStringLiteral("org.freedesktop.appearance")
                                     .startsWith(filter.chopped(1))))
      return {{"org.freedesktop.appearance", values_}};
  }
  return namespaces.isEmpty()
             ? PortalSettingsMap{{"org.freedesktop.appearance", values_}}
             : PortalSettingsMap{};
}
QDBusVariant SettingsPortal::Read(const QString &nameSpace,
                                  const QString &key) {
  refresh();
  if (nameSpace == "org.freedesktop.appearance" && values_.contains(key))
    return QDBusVariant(values_.value(key));
  auto *context = qobject_cast<FileChooserPortal *>(parent());
  if (context && context->calledFromDBus())
    context->sendErrorReply("org.freedesktop.portal.Error.NotFound",
                            "Unknown desktop setting.");
  return QDBusVariant(QVariant::fromValue(uint(0)));
}
} // namespace LunaDash
