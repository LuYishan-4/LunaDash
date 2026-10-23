#include "compositor/client/ClientWindow.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/Register.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include <QFile>
#include <iterator>
#include <string>

namespace LunaDash {
namespace {
bool belongsToProcess(qint64 candidate, qint64 launched) {
  if (launched <= 0)
    return false;
  for (int depth = 0; candidate > 1 && depth < 16; ++depth) {
    if (candidate == launched)
      return true;
    QFile file(QString("/proc/%1/stat").arg(candidate));
    if (!file.open(QIODevice::ReadOnly))
      return false;
    const auto stat = file.read(4096);
    const auto end = stat.lastIndexOf(')');
    if (end < 0)
      return false;
    const auto fields = stat.mid(end + 2).split(' ');
    if (fields.size() < 2)
      return false;
    const auto parent = fields[1].toLongLong();
    if (parent == candidate)
      return false;
    candidate = parent;
  }
  return false;
}
} // namespace

bool WaylandCompositor::toggleScratchpad(QString *error) {
  for (const auto &client : clients_) {
    if (client->id != scratchpadWindow_)
      continue;
    const bool hide = client->mapped && !client->minimized &&
                      client->workspace == workspace_ &&
                      focused_ == client.get();
    client->workspace = workspace_;
    client->minimized = hide;
    client->floating = true;
    client->maximized = false;
    arrange();
    if (hide)
      synchronizeWindowFocus();
    else
      focus(client.get());
    return true;
  }
  scratchpadWindow_ = 0;
  if (scratchpadPending_)
    return true;
  std::string nativeError;
  auto command = defaultApplicationCommand("terminal", &nativeError);
  if (error)
    *error = QString::fromStdString(nativeError);
  if (command.empty())
    return false;
  const auto executable = QString::fromStdString(command.front());
  QStringList arguments;
  for (auto argument = std::next(command.begin()); argument != command.end();
       ++argument)
    arguments.push_back(QString::fromStdString(*argument));
  auto *process = spawn(arguments, executable, false);
  if (!process) {
    if (error)
      *error = "Could not launch the configured terminal.";
    return false;
  }
  scratchpadPending_ = true;
  const auto generation = ++scratchpadGeneration_;
  scratchpadError_.clear();
  connect(process, &QProcess::started, this,
          [this, process] { scratchpadProcess_ = process->processId(); });
  connect(process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError errorCode) {
            if (errorCode == QProcess::FailedToStart) {
              d->cancelScratchpadTimeout();
              scratchpadPending_ = false;
              scratchpadProcess_ = 0;
              scratchpadError_ = "Could not launch the configured terminal.";
            }
          });
  if (!d->scheduleScratchpadTimeout(generation)) {
    scratchpadPending_ = false;
    scratchpadError_ = "Could not monitor the configured terminal launch.";
  }
  return true;
}

void WaylandCompositor::adoptScratchpad(ClientWindow *client) {
  if (client && client->id == scratchpadWindow_) {
    client->floating = true;
    return;
  }
  if (!client || !scratchpadPending_ || client->utility ||
      !belongsToProcess(client->processId, scratchpadProcess_))
    return;
  scratchpadWindow_ = client->id;
  d->cancelScratchpadTimeout();
  scratchpadPending_ = false;
  scratchpadProcess_ = 0;
  client->floating = true;
  client->workspace = workspace_;
  client->preferredFloatingSize = QSize(960, 540);
}
} // namespace LunaDash
