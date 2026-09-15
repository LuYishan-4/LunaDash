#include <LuDash/localization/Localization.h>
#include <LuDash/packages/PackageManager.h>
#include <LuDash/default_applications/DefaultApplications.h>
#include <QtWidgets>
#include <QProcess>
namespace LuDash {
QStringList packageTransactionArguments(const QString& operation, const QString& package) {
    if (operation == "upgrade") return {"-Syu"};
    static const QRegularExpression valid("^[a-zA-Z0-9@_+][a-zA-Z0-9@._+:-]*$");
    if (!valid.match(package).hasMatch()) return {};
    if (operation == "install") return {"-Syu", "--", package};
    if (operation == "remove") return {"-R", "--", package};
    return {};
}
QWidget* createPackageManager() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* description = new QLabel(LuDash::translate("pacman: queries run without administrator privileges. Confirm installation and removal in a terminal."));
    description->setWordWrap(true); layout->addWidget(description);
    auto* package = new QLineEdit; package->setPlaceholderText(LuDash::translate("Package name, for example fcitx5")); layout->addWidget(package);
    auto* row = new QHBoxLayout; layout->addLayout(row);
    auto* installed = new QPushButton(LuDash::translate("Installed")); auto* search = new QPushButton(LuDash::translate("Search"));
    auto* install = new QPushButton(LuDash::translate("Install")); auto* remove = new QPushButton(LuDash::translate("Remove")); auto* upgrade = new QPushButton(LuDash::translate("Full upgrade"));
    for (auto* button : {installed, search, install, remove, upgrade}) row->addWidget(button);
    auto* output = new QPlainTextEdit; output->setReadOnly(true); output->setMaximumBlockCount(12000); layout->addWidget(output, 1);
    const auto pacman = QStandardPaths::findExecutable("pacman");
    if (pacman.isEmpty()) {
        output->setPlainText(LuDash::translate("pacman is not available on this system. Other desktop features remain available."));
        for (auto* button : {installed, search, install, remove, upgrade}) button->setEnabled(false);
        return page;
    }
    auto* process = new QProcess(page); process->setProcessChannelMode(QProcess::MergedChannels);
    auto query = [=](const QStringList& arguments) {
        if (process->state() != QProcess::NotRunning) return;
        output->clear(); process->start(pacman, arguments);
    };
    QObject::connect(process, &QProcess::readyReadStandardOutput, page, [=] { output->moveCursor(QTextCursor::End); output->insertPlainText(QString::fromLocal8Bit(process->readAllStandardOutput())); });
    QObject::connect(process, &QProcess::errorOccurred, page, [=](QProcess::ProcessError) { output->appendPlainText(process->errorString()); });
    QObject::connect(process, &QProcess::finished, page, [=](int code, QProcess::ExitStatus) { output->appendPlainText(QString(LuDash::translate("\n[Exit code %1]")).arg(code)); });
    QObject::connect(installed, &QPushButton::clicked, page, [=] { query({"-Q"}); });
    QObject::connect(search, &QPushButton::clicked, page, [=] { if (!package->text().trimmed().isEmpty()) query({"-Ss", "--", package->text().trimmed()}); });
    auto transaction = [=](const QString& operation) {
        const auto arguments = packageTransactionArguments(operation, package->text().trimmed());
        if (arguments.isEmpty()) { QMessageBox::warning(page, LuDash::translate("Invalid package name"), LuDash::translate("Enter one valid package name.")); return; }
        const QString sudo = QStandardPaths::findExecutable("sudo");
        if (sudo.isEmpty()) { QMessageBox::warning(page, LuDash::translate("sudo not found"), LuDash::translate("Ask your system administrator to manage packages.")); return; }
        for (const auto& terminal : {QString("konsole"), QString("foot"), QString("alacritty")}) {
            const auto path = QStandardPaths::findExecutable(terminal);
            if (path.isEmpty()) continue;
            if (QMessageBox::question(page, LuDash::translate("Confirm in terminal"), LuDash::translate("The terminal will run:\n") + sudo + " " + pacman + " " + arguments.join(' ')) != QMessageBox::Yes) return;
            QStringList terminalArguments;
            if (terminal == "konsole") {
                terminalArguments = LuDash::konsoleCommand();
                terminalArguments.removeFirst();
                terminalArguments << "-e";
            } else {
                terminalArguments << "-e";
            }
            terminalArguments << sudo << pacman;
            terminalArguments += arguments;
            if (!QProcess::startDetached(path, terminalArguments)) QMessageBox::warning(page, LuDash::translate("Launch failed"), path);
            return;
        }
        QMessageBox::information(page, LuDash::translate("Terminal required"), LuDash::translate("Install Konsole, foot, or Alacritty to manage packages."));
    };
    QObject::connect(install, &QPushButton::clicked, page, [=] { transaction("install"); });
    QObject::connect(remove, &QPushButton::clicked, page, [=] { transaction("remove"); });
    QObject::connect(upgrade, &QPushButton::clicked, page, [=] { transaction("upgrade"); });
    QObject::connect(page, &QObject::destroyed, process, [process] { process->disconnect(); if (process->state() != QProcess::NotRunning) { process->kill(); process->waitForFinished(1000); } });
    query({"-Q"});
    return page;
}
}
