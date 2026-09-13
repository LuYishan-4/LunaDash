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
    const auto cpuText = readStatusFile("/proc/stat");
    LuDashCpuCounters counters{};
    int cpu = 0;
    if (ludash_parse_cpu(cpuText.constData(), static_cast<size_t>(cpuText.size()), &counters)) {
        cpu = ludash_cpu_percent(previous_, counters); previous_ = counters;
    }
    history_.append(cpu); if (history_.size() > 20) history_.removeFirst();
    data_["cpuPercent"] = cpu; data_["cpuHistory"] = history_;
    const auto memoryText = readStatusFile("/proc/meminfo");
    LuDashMemoryCounters memory{};
    ludash_parse_memory(memoryText.constData(), static_cast<size_t>(memoryText.size()), &memory);
    data_["memoryUsed"] = static_cast<double>(ludash_memory_used(memory)) / (1024.0 * 1024.0);
    data_["memoryTotal"] = static_cast<double>(memory.total_kib) / (1024.0 * 1024.0);
    data_["memoryPercent"] = ludash_memory_percent(memory);
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
