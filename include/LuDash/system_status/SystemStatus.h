#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QtGlobal>
namespace LuDash {
class SystemStatus final : public QObject {
public:
    explicit SystemStatus(QObject* parent = nullptr);
    QJsonObject snapshot() const;
private:
    void refresh();
    QJsonObject data_;
    QJsonArray history_;
    quint64 previousTotal_ = 0;
    quint64 previousIdle_ = 0;
};
}
