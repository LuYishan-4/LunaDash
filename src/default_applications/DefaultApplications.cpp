#include <LuDash/default_applications/DefaultApplications.h>
#include <LuDash/configuration/DesktopPreferences.h>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
int qInitResources_terminal_profile();
namespace LuDash {
namespace {
bool validCommand(const QJsonValue& value) {
    if (!value.isArray() || value.toArray().size() > 24) return false;
    const auto arguments = value.toArray();
    for (const auto& argument : arguments) if (!argument.isString() || argument.toString().size() > 1024 || argument.toString().contains(QChar(0))) return false;
    if (arguments.isEmpty()) return true;
    const auto program = arguments.first().toString();
    return !program.isEmpty() && !program.startsWith('-') && QFileInfo(program).fileName() != "ludash-desktop" && QFileInfo(program).fileName() != "ludashctl";
}
}
QJsonObject defaultApplications() {
    QJsonObject result;
    for (const auto& role : {"terminal", "files"}) {
        const auto value = QJsonValue::fromVariant(QSettings().value(QString("defaultApps/") + role, QStringList{}));
        result[role] = validCommand(value) ? value : QJsonValue(QJsonArray{});
    }
    return result;
}
bool setDefaultApplications(const QJsonObject& changes, QString* error) {
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        if ((it.key() != "terminal" && it.key() != "files") || !validCommand(it.value())) { if (error) *error = "Defaults require terminal/files argument arrays (empty selects LuDash), at most 24 strings, no recursive LuDash launcher."; return false; }
        const auto command = it.value().toArray();
        if (!command.isEmpty() && QStandardPaths::findExecutable(command.first().toString()).isEmpty()) { if (error) *error = "Application executable was not found."; return false; }
    }
    QSettings settings;
    for (auto it = changes.begin(); it != changes.end(); ++it) settings.setValue("defaultApps/" + it.key(), it.value().toVariant());
    settings.sync();
    if (settings.status() != QSettings::NoError) { if (error) *error = "Could not save default applications."; return false; }
    return true;
}
QStringList defaultApplicationCommand(const QString& role, QString* error) {
    if (role != "terminal" && role != "files") { if (error) *error = "Unknown application role."; return {}; }
    const auto configured = defaultApplications().value(role).toArray(); QStringList result;
    for (const auto& argument : configured) result << argument.toString();
    if (!result.isEmpty()) return result;
    if (role == "files") return {};
    const auto terminal = QStandardPaths::findExecutable("konsole"), fish = QStandardPaths::findExecutable("fish");
    if (terminal.isEmpty() || fish.isEmpty()) { if (error) *error = "Install konsole and fish, or select another terminal command in Settings > Applications."; return {}; }
    ::qInitResources_terminal_profile();
    QFile profile(":/LuDash/data/terminal/ludash.fish");
    if (!profile.open(QIODevice::ReadOnly)) { if (error) *error = "Could not read the LuDash Fish profile."; return {}; }
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/konsole";
    QDir().mkpath(directory); const auto path = directory + "/LuDashGenerated.colorscheme";
    if (QFileInfo(path).isSymLink()) { if (error) *error = "Refusing to replace a symlinked terminal profile."; return {}; }
    const auto hex = desktopPreferences().value("accent").toString();
    const auto accent = QString("%1,%2,%3").arg(hex.mid(1, 2).toInt(nullptr, 16)).arg(hex.mid(3, 2).toInt(nullptr, 16)).arg(hex.mid(5, 2).toInt(nullptr, 16));
    QString scheme = "# Generated only for LuDash. Use a custom terminal command for your own profile.\n[General]\nDescription=LuDash Generated\nOpacity=0.96\nBlur=false\n";
    const QStringList colors{"18,26,36", "226,153,169", "146,204,178", "225,206,152", accent, "191,174,219", accent, "226,233,241"};
    for (const auto& variant : {"", "Intense", "Faint"}) {
        scheme += QString("[Background%1]\nColor=18,26,36\n[Foreground%1]\nColor=226,233,241\n").arg(variant);
        for (int index = 0; index < colors.size(); ++index) scheme += QString("[Color%1%2]\nColor=%3\n").arg(index).arg(variant, colors[index]);
    }
    const auto contents = scheme.toUtf8();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner) || file.write(contents) != contents.size() || !file.commit()) { if (error) *error = "Could not save the LuDash terminal profile."; return {}; }
    return {terminal, "--separate", "--builtin-profile", "--hide-menubar", "--hide-tabbar", "-p", "ColorScheme=LuDashGenerated", "-p", "Font=monospace,11,-1,5,50,0,0,0,0,0", "-p", "TerminalMargin=16", "-p", "LocalTabTitleFormat=LuDash Terminal", "-e", fish, "--interactive", "--init-command", QString::fromUtf8(profile.readAll())};
}
}
