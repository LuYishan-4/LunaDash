#include "config/Localization/Localization.hpp"
#include "desktop/Launcher/Launcher.hpp"
#include "desktop/ApplicationCatalog/ApplicationCatalog.hpp"
#include <QtWidgets>
#include <QProcess>
namespace LuDash {
QWidget* createLauncher() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* search = new QLineEdit; search->setPlaceholderText(LuDash::translate("Search applications...")); layout->addWidget(search);
    auto* list = new QListWidget; layout->addWidget(list);
    const auto apps = discoverApplications();
    const auto builtins = builtinApplications();
    auto fill = [=](const QString& filter) {
        list->clear();
        for (const auto& entry : builtins) if (entry.name.contains(filter, Qt::CaseInsensitive) || entry.id.contains(filter, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem("LuDash " + entry.name, list); item->setData(Qt::UserRole, entry.id);
        }
        for (int i = 0; i < apps.size(); ++i) if (apps[i].name.contains(filter, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem(QIcon::fromTheme(apps[i].icon), apps[i].name, list); item->setData(Qt::UserRole, QString::number(i));
        }
        if (list->count()) list->setCurrentRow(0);
    };
    fill(""); QObject::connect(search, &QLineEdit::textChanged, page, fill);
    auto activate = [=](QListWidgetItem* item) {
        if (!item) return;
        bool external = false; const auto id = item->data(Qt::UserRole).toString(); const int index = id.toInt(&external);
        const bool ok = external ? QProcess::startDetached(apps[index].program, apps[index].arguments, apps[index].workingDirectory)
                                 : QProcess::startDetached(QCoreApplication::applicationFilePath(), {"--app", id});
        if (ok) page->window()->close(); else QMessageBox::warning(page, LuDash::translate("Launch failed"), item->text());
    };
    QObject::connect(list, &QListWidget::itemActivated, page, activate);
    QObject::connect(list, &QListWidget::itemClicked, page, activate);
    QObject::connect(search, &QLineEdit::returnPressed, page, [=] { activate(list->currentItem()); });
    return page;
}
}
