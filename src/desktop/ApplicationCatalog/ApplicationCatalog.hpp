#pragma once
#include <QString>
#include <QStringList>
#include <QList>
namespace LuDash {
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
}
