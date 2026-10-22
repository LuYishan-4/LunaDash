#pragma once
#include <QJsonObject>
#include <QObject>
#include <functional>
class QLocalServer;
namespace LunaDash {
class ControlServer final : public QObject {
public:
  ControlServer(const QString &path,
                std::function<QJsonObject(const QJsonObject &)> handler,
                QObject *parent);

private:
  QLocalServer *server_;
};
} // namespace LunaDash
