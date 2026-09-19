#pragma once
#include <QString>
#include <QStringList>
#include <functional>
class QWidget;
namespace LunaDash {
void openAssociatedFiles(QWidget *parent, const QStringList &files,
                         bool chooseApplication,
                         const std::function<void(const QString &)> &report);
void chooseFileDefault(QWidget *parent, const QString &file,
                       const std::function<void(const QString &)> &report);
void showFileAssociationSettings(QWidget *parent);
void showFileManagerFirstRun(QWidget *parent);
bool migrateLegacyFileAssociations(QString *error = nullptr);
} // namespace LunaDash
