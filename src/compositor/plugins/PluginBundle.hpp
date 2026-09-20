#pragma once
#include <QString>
#include <QTemporaryDir>
#include <memory>
namespace LunaDash {
// A private copy keeps running code/assets stable while the installed package
// is rebuilt. Each revision has a fresh URL and dynamic-library filename.
class PluginBundle {
public:
  static QString fingerprint(const QString &directory, QString *error);
  static std::shared_ptr<PluginBundle> copy(const QString &directory,
                                            QString *error);
  QString file(const QString &name) const;

private:
  QTemporaryDir directory_;
};
} // namespace LunaDash
