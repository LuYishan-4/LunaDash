#include <LuDash/system_status/SystemStatus.h>
#include <QDir>
#include <QFile>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>
#include <algorithm>
namespace LuDash {
namespace {
QByteArray readStatusFile(const QString& path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.read(128 * 1024).trimmed() : QByteArray{};
}
}
SystemStatus::SystemStatus(QObject* parent) : QObject(parent) {
    data_ = {{"os", QSysInfo::prettyProductName()}, {"kernel", QSysInfo::kernelVersion()},
             {"architecture", QSysInfo::currentCpuArchitecture()}, {"host", QSysInfo::machineHostName()},
             {"user", qEnvironmentVariable("USER")}};
    for (const auto& line : readStatusFile("/proc/cpuinfo").split('\n')) {
        if (line.startsWith("model name") || line.startsWith("Hardware")) {
            data_["cpuModel"] = QString::fromUtf8(line.mid(line.indexOf(':') + 1).trimmed()); break;
        }
    }
    refresh();
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &SystemStatus::refresh);
    timer->start(1500);
}
QJsonObject SystemStatus::snapshot() const { return data_; }
void SystemStatus::refresh() {
    const auto fields = readStatusFile("/proc/stat").split('\n').value(0).simplified().split(' ');
    quint64 total = 0;
    for (qsizetype i = 1; i < std::min<qsizetype>(9, fields.size()); ++i) total += fields[i].toULongLong();
    const quint64 idle = fields.value(4).toULongLong() + fields.value(5).toULongLong();
    int cpu = 0;
    if (previousTotal_ && total > previousTotal_ && idle >= previousIdle_) {
        const double busy = 1.0 - static_cast<double>(idle - previousIdle_) / static_cast<double>(total - previousTotal_);
        cpu = std::clamp(static_cast<int>(busy * 100.0), 0, 100);
    }
    previousTotal_ = total; previousIdle_ = idle;
    history_.append(cpu); if (history_.size() > 20) history_.removeFirst();
    data_["cpuPercent"] = cpu; data_["cpuHistory"] = history_;
    quint64 memoryTotal = 0, available = 0;
    for (const auto& line : readStatusFile("/proc/meminfo").split('\n')) {
        const auto values = line.simplified().split(' ');
        if (line.startsWith("MemTotal:")) memoryTotal = values.value(1).toULongLong();
        if (line.startsWith("MemAvailable:")) available = values.value(1).toULongLong();
    }
    const quint64 used = memoryTotal - std::min(memoryTotal, available);
    data_["memoryUsed"] = static_cast<double>(used) / (1024.0 * 1024.0);
    data_["memoryTotal"] = static_cast<double>(memoryTotal) / (1024.0 * 1024.0);
    data_["memoryPercent"] = memoryTotal ? static_cast<int>(100.0 * static_cast<double>(used) / static_cast<double>(memoryTotal)) : 0;
    const QStorageInfo storage(QDir::homePath());
    data_["diskUsed"] = static_cast<double>(storage.bytesTotal() - storage.bytesAvailable()) / (1024.0 * 1024.0 * 1024.0);
    data_["diskTotal"] = static_cast<double>(storage.bytesTotal()) / (1024.0 * 1024.0 * 1024.0);
    int battery = -1;
    for (const auto& entry : QDir("/sys/class/power_supply").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto base = "/sys/class/power_supply/" + entry + "/";
        if (readStatusFile(base + "type") != "Battery") continue;
        bool valid = false;
        const int capacity = readStatusFile(base + "capacity").toInt(&valid);
        if (valid) { battery = std::clamp(capacity, 0, 100); break; }
    }
    data_["batteryPercent"] = battery;
}
}
