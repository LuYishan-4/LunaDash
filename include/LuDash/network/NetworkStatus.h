#pragma once
#include <QJsonObject>
#include <QObject>
namespace LuDash {
QJsonObject describeNetwork(unsigned int state, unsigned int connectivity);
class NetworkStatus final : public QObject {
public:
    explicit NetworkStatus(QObject* parent = nullptr);
    QJsonObject snapshot() const;
private:
    void refresh();
    QJsonObject status_;
    bool pending_ = false;
};
}
