#include "config/Localization/Localization.hpp"
#include "desktop/SystemMonitor/SystemMonitor.hpp"
#include <QtWidgets>
#include <QStorageInfo>
#include "compositor/SystemMetrics/SystemMetrics.h"

namespace LuDash {
QWidget* createSystemMonitor() {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    auto* title = new QLabel(LuDash::translate("System monitor")); title->setObjectName("heading");
    layout->addWidget(title);
    auto* info = new QLabel; info->setWordWrap(true); layout->addWidget(info);
    auto* memory = new QProgressBar; memory->setRange(0, 100); layout->addWidget(memory);
    auto* disk = new QProgressBar; disk->setRange(0, 100); layout->addWidget(disk);
    layout->addStretch();
    auto refresh = [=] {
        QFile file("/proc/meminfo");
        LuDashMemoryCounters counters{};
        if (file.open(QIODevice::ReadOnly)) {
            const auto text = file.read(128 * 1024);
            ludash_parse_memory(text.constData(), static_cast<size_t>(text.size()), &counters);
        }
        memory->setValue(ludash_memory_percent(counters));
        memory->setFormat(QString(LuDash::translate("Memory  %1 / %2 GiB  (%p%)")).arg(static_cast<double>(ludash_memory_used(counters)) / 1048576.0, 0, 'f', 1).arg(static_cast<double>(counters.total_kib) / 1048576.0, 0, 'f', 1));
        const QStorageInfo storage(QDir::homePath());
        disk->setValue(storage.bytesTotal() > 0 ? int(100.0 * static_cast<double>(storage.bytesTotal() - storage.bytesAvailable()) / static_cast<double>(storage.bytesTotal())) : 0);
        disk->setFormat(LuDash::translate("Home storage  %p%"));
        info->setText(QString(LuDash::translate("%1\nKernel  %2 · %3\nHost  %4\n\nLuDash Desktop 0.1 · C++20 / Qt 6 / OpenGL"))
            .arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion(), QSysInfo::currentCpuArchitecture(), QSysInfo::machineHostName()));
    };
    auto* timer = new QTimer(page); QObject::connect(timer, &QTimer::timeout, page, refresh); timer->start(2000); refresh();
    return page;
}
}
