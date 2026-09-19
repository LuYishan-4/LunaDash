#include "desktop/package/PackageManager.hpp"
#include "config/localization/Localization.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include <QProcess>
#include <QtWidgets>
namespace LunaDash {
QStringList packageTransactionArguments(const QString &operation,
                                        const QString &package) {
  if (operation == "upgrade")
    return {"-Syu"};
  static const QRegularExpression valid("^[a-zA-Z0-9@_+][a-zA-Z0-9@._+:-]*$");
  if (!valid.match(package).hasMatch())
    return {};
  if (operation == "install")
    return {"-Syu", "--", package};
  if (operation == "remove")
    return {"-R", "--", package};
  return {};
}
QWidget *createPackageManager() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *description = new QLabel(LunaDash::translate(
      "pacman: queries run without administrator privileges. Confirm "
      "installation and removal in a terminal."));
  description->setWordWrap(true);
  layout->addWidget(description);
  auto *package = new QLineEdit;
  package->setPlaceholderText(
      LunaDash::translate("Package name, for example fcitx5"));
  layout->addWidget(package);
  auto *row = new QHBoxLayout;
  layout->addLayout(row);
  auto *installed = new QPushButton(LunaDash::translate("Installed"));
  auto *search = new QPushButton(LunaDash::translate("Search"));
  auto *install = new QPushButton(LunaDash::translate("Install"));
  auto *remove = new QPushButton(LunaDash::translate("Remove"));
  auto *upgrade = new QPushButton(LunaDash::translate("Full upgrade"));
  for (auto *button : {installed, search, install, remove, upgrade})
    row->addWidget(button);
  auto *output = new QPlainTextEdit;
  output->setReadOnly(true);
  output->setMaximumBlockCount(12000);
  layout->addWidget(output, 1);
  const auto pacman = QStandardPaths::findExecutable("pacman");
  if (pacman.isEmpty()) {
    output->setPlainText(
        LunaDash::translate("pacman is not available on this system. Other "
                            "desktop features remain available."));
    for (auto *button : {installed, search, install, remove, upgrade})
      button->setEnabled(false);
    return page;
  }
  auto *process = new QProcess(page);
  process->setProcessChannelMode(QProcess::MergedChannels);
  auto query = [=](const QStringList &arguments) {
    if (process->state() != QProcess::NotRunning)
      return;
    output->clear();
    process->start(pacman, arguments);
  };
  QObject::connect(process, &QProcess::readyReadStandardOutput, page, [=] {
    output->moveCursor(QTextCursor::End);
    output->insertPlainText(
        QString::fromLocal8Bit(process->readAllStandardOutput()));
  });
  QObject::connect(process, &QProcess::errorOccurred, page,
                   [=](QProcess::ProcessError) {
                     output->appendPlainText(process->errorString());
                   });
  QObject::connect(
      process, &QProcess::finished, page, [=](int code, QProcess::ExitStatus) {
        output->appendPlainText(
            QString(LunaDash::translate("\n[Exit code %1]")).arg(code));
      });
  QObject::connect(installed, &QPushButton::clicked, page,
                   [=] { query({"-Q"}); });
  QObject::connect(search, &QPushButton::clicked, page, [=] {
    if (!package->text().trimmed().isEmpty())
      query({"-Ss", "--", package->text().trimmed()});
  });
  auto transaction = [=](const QString &operation) {
    const auto arguments =
        packageTransactionArguments(operation, package->text().trimmed());
    if (arguments.isEmpty()) {
      QMessageBox::warning(
          page, LunaDash::translate("Invalid package name"),
          LunaDash::translate("Enter one valid package name."));
      return;
    }
    const QString sudo = QStandardPaths::findExecutable("sudo");
    if (sudo.isEmpty()) {
      QMessageBox::warning(
          page, LunaDash::translate("sudo not found"),
          LunaDash::translate(
              "Ask your system administrator to manage packages."));
      return;
    }
    for (const auto &terminal :
         {QString("konsole"), QString("foot"), QString("alacritty")}) {
      const auto path = QStandardPaths::findExecutable(terminal);
      if (path.isEmpty())
        continue;
      if (QMessageBox::question(
              page, LunaDash::translate("Confirm in terminal"),
              LunaDash::translate("The terminal will run:\n") + sudo + " " +
                  pacman + " " + arguments.join(' ')) != QMessageBox::Yes)
        return;
      QStringList terminalArguments;
      if (terminal == "konsole") {
        terminalArguments = LunaDash::konsoleCommand();
        terminalArguments.removeFirst();
        terminalArguments << "-e";
      } else {
        terminalArguments << "-e";
      }
      terminalArguments << sudo << pacman;
      terminalArguments += arguments;
      if (!QProcess::startDetached(path, terminalArguments))
        QMessageBox::warning(page, LunaDash::translate("Launch failed"), path);
      return;
    }
    QMessageBox::information(
        page, LunaDash::translate("Terminal required"),
        LunaDash::translate(
            "Install Konsole, foot, or Alacritty to manage packages."));
  };
  QObject::connect(install, &QPushButton::clicked, page,
                   [=] { transaction("install"); });
  QObject::connect(remove, &QPushButton::clicked, page,
                   [=] { transaction("remove"); });
  QObject::connect(upgrade, &QPushButton::clicked, page,
                   [=] { transaction("upgrade"); });
  QObject::connect(page, &QObject::destroyed, process, [process] {
    process->disconnect();
    if (process->state() != QProcess::NotRunning) {
      process->kill();
      process->waitForFinished(1000);
    }
  });
  query({"-Q"});
  return page;
}
} // namespace LunaDash
