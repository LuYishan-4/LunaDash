#include "ctl/command/ControlClient.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

namespace LunaDash {
namespace {
// The compositor publishes LUNADASH_CONTROL to everything it starts, but a
// terminal, editor or shell that was started outside that tree does not inherit
// it. Fall back to the session's standard control sockets, then to the single
// control socket in the runtime directory, so lunadashctl works from anywhere
// in the session.
QString defaultControlPath() {
  const auto runtime =
      QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
  if (runtime.isEmpty())
    return {};
  for (const auto &name : {QStringLiteral("lunadash-0-control"),
                           QStringLiteral("ludash-0-control")}) {
    const auto candidate = runtime + '/' + name;
    if (QFileInfo::exists(candidate))
      return candidate;
  }
  // A nested session or a custom --socket exposes one differently named control
  // socket. Several of them are ambiguous, so an explicit variable is required.
  QString found;
  const auto entries = QDir(runtime).entryInfoList(
      {QStringLiteral("*-control")},
      QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
  for (const auto &entry : entries) {
    if (!found.isEmpty())
      return {};
    found = entry.absoluteFilePath();
  }
  return found;
}
} // namespace

int ControlClient::run(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  const auto args = application.arguments();
  if (args.size() < 2) {
    QTextStream(stderr)
        << "Usage: lunadashctl "
           "status|workspace|focus|group-window|expel-window|minimize|close|"
           "language|shortcut-capture|shortcuts|reset-shortcuts|check-update|"
           "capture|screenshot|"
           "wallpaper|choose-wallpaper|wallpaper-image|wallpaper-default|"
           "appearance|setup|finish-setup|configure-network|launch-x11|"
           "open-settings|system-tool|audio|network|power-profile|desktop-size|"
           "reset-preferences|default-apps|launch-default|module-validate|"
           "module-save|module-template|module-code-trust|module-reset|quit "
           "[value]\n"
           "  group-window value: {\"window\":ID,\"target\":ID}\n";
    return 2;
  }

  auto path = qEnvironmentVariable("LUNADASH_CONTROL");
  if (path.isEmpty())
    path = qEnvironmentVariable("LUDASH_CONTROL");
  if (path.isEmpty())
    path = defaultControlPath();
  if (path.isEmpty()) {
    QTextStream(stderr)
        << "LUNADASH_CONTROL is not set and no single LunaDash control socket "
           "was found in the runtime directory.\n";
    return 2;
  }

  QLocalSocket socket;
  socket.connectToServer(path);
  if (!socket.waitForConnected(1500))
    return 1;
  socket.write(
      QJsonDocument(QJsonObject{{"method", args[1]}, {"value", args.value(2)}})
          .toJson(QJsonDocument::Compact) +
      '\n');
  socket.flush();

  QByteArray result;
  while (!result.contains('\n')) {
    if (!socket.bytesAvailable() && !socket.waitForReadyRead(1500))
      return 1;
    result += socket.readAll();
    if (result.size() > 1024 * 1024)
      return 1;
  }
  QTextStream(stdout) << result;
  return QJsonDocument::fromJson(result).object().contains("error") ? 1 : 0;
}

} // namespace LunaDash
