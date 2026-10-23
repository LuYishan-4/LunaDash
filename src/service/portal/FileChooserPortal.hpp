#pragma once

#include <gio/gio.h>

namespace LunaDash {

class FileChooserPortal final {
public:
  FileChooserPortal(GDBusConnection *connection,
                    GDBusNodeInfo *introspection);
  ~FileChooserPortal();

  FileChooserPortal(const FileChooserPortal &) = delete;
  FileChooserPortal &operator=(const FileChooserPortal &) = delete;

  bool registerObject(GError **error);
  GDBusNodeInfo *introspection() const { return introspection_; }

private:
  GDBusConnection *connection_ = nullptr;
  GDBusNodeInfo *introspection_ = nullptr;
  guint registration_ = 0;
};

} // namespace LunaDash
