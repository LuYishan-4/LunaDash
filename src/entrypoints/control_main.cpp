#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTextStream>

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  const auto args = application.arguments();
  if (args.size() < 2) {
    QTextStream(stderr)
        << "Usage: lunadahctl "
           "status|workspace|focus|group-window|expel-window|minimize|close|"
           "language|shortcut-capture|shortcuts|reset-shortcuts|check-update|"
           "wallpaper|choose-wallpaper|wallpaper-image|wallpaper-default|"
           "appearance|setup|finish-setup|configure-network|launch-x11|"
           "open-settings|system-tool|audio|power-profile|desktop-size|"
           "reset-preferences|default-apps|launch-default|module-validate|"
           "module-save|module-template|module-code-trust|module-reset|quit "
           "[value]\n"
           "  group-window value: {\"window\":ID,\"target\":ID}\n";
    return 2;
  }

  auto path = qEnvironmentVariable("LUNADAH_CONTROL");
  if (path.isEmpty())
    path = qEnvironmentVariable("LUDASH_CONTROL");
  if (path.isEmpty()) {
    QTextStream(stderr) << "LUNADAH_CONTROL is not set.\n";
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
