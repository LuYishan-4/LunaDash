#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <LuDash/system_metrics/SystemMetrics.h>
namespace LuDash {
class SystemStatus final : public QObject {
public:
    explicit SystemStatus(QObject* parent = nullptr);
    QJsonObject snapshot() const;
private:
    void refresh();
    QJsonObject data_;
    QJsonArray history_;
    LuDashCpuCounters previous_{};
};
}
