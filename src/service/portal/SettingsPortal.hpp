#pragma once

#include <gio/gio.h>

#include <cstdint>
#include <map>
#include <string>

namespace LunaDash {

class SettingsPortal final {
public:
  SettingsPortal(GDBusConnection *connection, GDBusNodeInfo *introspection);
  ~SettingsPortal();

  SettingsPortal(const SettingsPortal &) = delete;
  SettingsPortal &operator=(const SettingsPortal &) = delete;

  bool registerObject(GError **error);

  static void methodCall(GDBusConnection *, const gchar *, const gchar *,
                         const gchar *, const gchar *, GVariant *,
                         GDBusMethodInvocation *, gpointer);
  static GVariant *getProperty(GDBusConnection *, const gchar *, const gchar *,
                               const gchar *, const gchar *, GError **,
                               gpointer);

private:
  GDBusConnection *connection_ = nullptr;
  GDBusNodeInfo *introspection_ = nullptr;
  GFileMonitor *monitor_ = nullptr;
  guint registration_ = 0;
  guint debounce_ = 0;
  guint refreshTimer_ = 0;
  std::map<std::string, std::uint32_t> values_;

  void refresh();
  void watchSettings();
  void scheduleRefresh();
};

} // namespace LunaDash
