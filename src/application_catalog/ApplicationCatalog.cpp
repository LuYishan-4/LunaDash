#include <LuDash/localization/Localization.h>
#include <LuDash/application_catalog/ApplicationCatalog.h>
#include <QtWidgets>
#include <QProcess>

namespace LuDash {
QList<BuiltinApplication> builtinApplications() {
    return {{"welcome", "◈", LuDash::translate("Welcome")}, {"files", "▱", LuDash::translate("Files")}, {"console", "❯", LuDash::translate("Command console")},
            {"monitor", "▥", LuDash::translate("System monitor")}, {"settings", "⚙", LuDash::translate("Settings")},
            {"packages", "⬡", LuDash::translate("Package manager")}, {"plugins", "◇", LuDash::translate("Plugins")}};
}
QList<ApplicationEntry> discoverApplications() {
    QList<ApplicationEntry> entries;
    QSet<QString> seen;
    auto roots = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    for (const auto& root : roots) {
        QDirIterator iterator(root, {"*.desktop"}, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const auto path = iterator.next();
            QString id = QDir(root).relativeFilePath(path).replace('/', '-');
            if (seen.contains(id)) continue;
            seen.insert(id);
            QSettings file(path, QSettings::IniFormat);
            file.beginGroup("Desktop Entry");
            if (file.value("Type").toString() != "Application" || file.value("Hidden").toBool()
                || file.value("NoDisplay").toBool() || file.value("Terminal").toBool()) continue;
            // Qt's argument tokenizer does not implement all Desktop Entry escaping rules.
            // Reject field codes requiring files/URLs rather than inventing an input.
            const auto command = file.value("Exec").toString();
            auto args = QProcess::splitCommand(command);
            if (args.isEmpty()) continue;
            const auto name = (selectedLanguage() == "zh_TW" ? file.value("Name[zh_TW]", file.value("Name")) : file.value("Name")).toString();
            const auto icon = file.value("Icon").toString();
            QStringList expanded;
            bool unsupported = false;
            for (auto arg : args) {
                if (arg == "%f" || arg == "%F" || arg == "%u" || arg == "%U") continue;
                if (arg == "%i") { if (!icon.isEmpty()) expanded << "--icon" << icon; continue; }
                arg.replace("%c", name).replace("%k", path).replace("%%", QString(QChar(1)));
                if (arg.contains('%')) { unsupported = true; break; }
                expanded << arg.replace(QChar(1), '%');
            }
            if (unsupported || expanded.isEmpty()) continue;
            auto program = expanded.takeFirst();
            if (QStandardPaths::findExecutable(program).isEmpty()) continue;
            entries.append({name, icon, program, expanded, file.value("Path").toString()});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.name.localeAwareCompare(b.name) < 0; });
    return entries;
}
}
