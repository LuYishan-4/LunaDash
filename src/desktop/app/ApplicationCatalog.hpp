#pragma once
#include <QList>
#include <QString>
#include <QStringList>
namespace LunaDash {
struct ApplicationEntry {
  QString name;
  QString icon;
  QString program;
  QStringList arguments;
  QString workingDirectory;
};
struct BuiltinApplication {
  QString id;
  QString symbol;
  QString name;
};
QList<BuiltinApplication> builtinApplications();
QList<ApplicationEntry> discoverApplications();
} // namespace LunaDash
