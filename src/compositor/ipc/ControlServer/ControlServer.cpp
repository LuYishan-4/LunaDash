#include "compositor/ipc/ControlServer/ControlServer.hpp"
#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QTimer>
#include <memory>
namespace LuDash {
ControlServer::ControlServer(const QString& path, std::function<QJsonObject(const QJsonObject&)> handler, QObject* parent) : QObject(parent), server_(new QLocalServer(this)) {
    server_->setSocketOptions(QLocalServer::UserAccessOption);
    if (!server_->listen(path)) qFatal("Cannot bind LuDash control socket; choose another --socket name.");
    connect(server_, &QLocalServer::newConnection, this, [this, handler] {
        while (auto* socket = server_->nextPendingConnection()) {
            socket->setReadBufferSize(65537);
            auto buffer = std::make_shared<QByteArray>();
            QTimer::singleShot(3000, socket, [socket] { socket->disconnectFromServer(); });
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [socket, buffer, handler] {
                buffer->append(socket->readAll());
                if (buffer->size() > 65536) { socket->abort(); return; }
                if (!buffer->contains('\n')) return;
                QJsonParseError error;
                const auto input = QJsonDocument::fromJson(buffer->left(buffer->indexOf('\n')), &error);
                const auto response = error.error == QJsonParseError::NoError && input.isObject() ? handler(input.object()) : QJsonObject{{"error", "Invalid request"}};
                socket->write(QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n'); socket->disconnectFromServer();
            });
        }
    });
}
}
