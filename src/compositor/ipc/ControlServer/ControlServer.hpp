#pragma once
#include <QObject>
#include <QJsonObject>
#include <functional>
class QLocalServer;
namespace LuDash {
class ControlServer final : public QObject {
public:
    ControlServer(const QString& path, std::function<QJsonObject(const QJsonObject&)> handler, QObject* parent);
private:
    QLocalServer* server_;
};
}
