#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QtConcurrent/QtConcurrent>
#include <type_traits>
#include <utility>

namespace LunaDash {

class RenderAsync final {
public:
  template <typename Work, typename Done>
  static auto run(QObject *receiver, Work &&work, Done &&done) {
    using Result = std::invoke_result_t<Work>;
    auto *watcher = new QFutureWatcher<Result>(receiver);
    QObject::connect(watcher, &QFutureWatcher<Result>::finished, receiver,
                     [watcher, callback = std::forward<Done>(done)]() mutable {
                       if constexpr (std::is_void_v<Result>) {
                         callback();
                       } else {
                         callback(watcher->future().result());
                       }
                       watcher->deleteLater();
                     });
    watcher->setFuture(QtConcurrent::run(std::forward<Work>(work)));
    return watcher;
  }
};

} // namespace LunaDash
