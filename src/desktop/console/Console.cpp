#include "desktop/console/Console.hpp"
#include "config/localization/Localization.hpp"
#include <QProcess>
#include <QtWidgets>
#include <csignal>
#include <unistd.h>

namespace LunaDash {
QWidget *createConsole() {
  auto *page = new QWidget;
  auto *layout = new QVBoxLayout(page);
  auto *hint =
      new QLabel(LunaDash::translate("COMMAND CONSOLE   /   NON-INTERACTIVE"));
  hint->setObjectName("muted");
  layout->addWidget(hint);
  auto *directory = new QLineEdit(QDir::homePath());
  directory->setPlaceholderText(LunaDash::translate("Working directory"));
  layout->addWidget(directory);
  auto *output = new QPlainTextEdit;
  output->setObjectName("consoleOutput");
  output->setReadOnly(true);
  output->setMaximumBlockCount(5000);
  output->setFont(QFont("monospace", 11));
  output->setPlainText(LunaDash::translate(
      "LuDash Console 0.1\nEnter a Linux command. Each command uses a separate "
      "shell.\nSet the working directory above. Use a full terminal for "
      "interactive programs.\n"));
  layout->addWidget(output, 1);
  auto *row = new QHBoxLayout;
  auto *input = new QLineEdit;
  input->setObjectName("consoleInput");
  input->setPlaceholderText(
      LunaDash::translate("Enter a command, for example uname -a"));
  auto *stop = new QPushButton(LunaDash::translate("Stop"));
  stop->setEnabled(false);
  row->addWidget(new QLabel("❯"));
  row->addWidget(input, 1);
  row->addWidget(stop);
  layout->addLayout(row);
  auto *process = new QProcess(page);
  process->setProcessChannelMode(QProcess::MergedChannels);
  process->setChildProcessModifier([] { ::setpgid(0, 0); });
  QObject::connect(page, &QObject::destroyed, process, [process] {
    process->disconnect();
    if (process->state() != QProcess::NotRunning) {
      const auto pid = process->processId();
      if (pid > 0)
        ::kill(-static_cast<pid_t>(pid), SIGKILL);
      process->kill();
      process->waitForFinished(1000);
    }
  });
  QObject::connect(input, &QLineEdit::returnPressed, page, [=] {
    if (input->text().trimmed().isEmpty() ||
        process->state() != QProcess::NotRunning)
      return;
    if (!QFileInfo(directory->text()).isDir()) {
      output->appendPlainText(
          LunaDash::translate("Invalid working directory."));
      return;
    }
    const auto command = input->text();
    output->appendPlainText("❯ " + command);
    input->clear();
    input->setEnabled(false);
    stop->setEnabled(true);
    process->setWorkingDirectory(directory->text());
    process->start("/bin/sh", {"-c", command});
    process->closeWriteChannel();
  });
  QObject::connect(process, &QProcess::readyReadStandardOutput, page, [=] {
    output->moveCursor(QTextCursor::End);
    output->insertPlainText(
        QString::fromLocal8Bit(process->readAllStandardOutput()));
    output->verticalScrollBar()->setValue(
        output->verticalScrollBar()->maximum());
  });
  QObject::connect(
      process, &QProcess::finished, page, [=](int code, QProcess::ExitStatus) {
        output->appendPlainText(
            QString(LunaDash::translate("\n[Exit code %1]")).arg(code));
        input->setEnabled(true);
        stop->setEnabled(false);
        input->setFocus();
      });
  QObject::connect(process, &QProcess::errorOccurred, page,
                   [=](QProcess::ProcessError) {
                     output->appendPlainText(process->errorString());
                     input->setEnabled(true);
                     stop->setEnabled(false);
                   });
  QObject::connect(stop, &QPushButton::clicked, process, [process] {
    const auto pid = process->processId();
    if (pid > 0)
      ::kill(-static_cast<pid_t>(pid), SIGKILL);
    process->kill();
  });
  return page;
}
} // namespace LunaDash
