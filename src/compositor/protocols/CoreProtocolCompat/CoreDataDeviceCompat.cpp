#include "compositor/protocols/CoreProtocolCompat/CoreDataDeviceCompat.hpp"

#if defined(LUDASH_HAS_QT_WAYLAND_PRIVATE)

#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandSurface>

// The Qt data-device implementation is fixed to protocol v1. Its private
// manager pointer is also where Qt expects clipboard integration to live, so
// replace that object in one isolated TU rather than advertising a second,
// competing wl_data_device_manager global.
#define private public
#include <QtWaylandCompositor/private/qwayland-server-wayland.h>
#include <QtWaylandCompositor/private/qwaylandcompositor_p.h>
#include <QtWaylandCompositor/private/qwldatadevicemanager_p.h>
#undef private

#include <algorithm>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>

namespace LuDash {
namespace {

constexpr int kDataDeviceVersion = 3;

class CoreDataDeviceManager;
class CoreDataDevice;

class CoreDataSource final : public QtWaylandServer::wl_data_source {
public:
  CoreDataSource(CoreDataDeviceManager *manager, wl_client *client, uint32_t id,
                 int version)
      : QtWaylandServer::wl_data_source(client, id, version),
        manager_(manager) {
    sources().insert(resource()->handle, this);
  }

  ~CoreDataSource() override {
    if (resource())
      sources().remove(resource()->handle);
    notifyDestroyed();
  }

  const QStringList &mimeTypes() const { return mimeTypes_; }
  uint32_t actions() const { return actions_; }

  void accept(const QString &mimeType) { send_target(mimeType); }

  void sendData(const QString &mimeType, int fd) {
    send_send(mimeType, fd);
    ::close(fd);
  }

  void cancel() { send_cancelled(); }

  static CoreDataSource *fromResource(wl_resource *resource) {
    return sources().value(resource, nullptr);
  }

protected:
  void data_source_offer(Resource *, const QString &mimeType) override {
    if (!mimeTypes_.contains(mimeType))
      mimeTypes_.append(mimeType);
  }

  void data_source_destroy(Resource *resource) override {
    wl_resource_destroy(resource->handle);
  }

  void data_source_set_actions(Resource *resource, uint32_t actions) override {
    constexpr uint32_t valid =
        WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
        WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
        WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    if (actions & ~valid) {
      wl_resource_post_error(resource->handle,
                             WL_DATA_SOURCE_ERROR_INVALID_ACTION_MASK,
                             "invalid wl_data_source action mask");
      return;
    }
    actions_ = actions;
  }

  void data_source_destroy_resource(Resource *) override { delete this; }

private:
  static QHash<wl_resource *, CoreDataSource *> &sources() {
    static QHash<wl_resource *, CoreDataSource *> map;
    return map;
  }

  void notifyDestroyed();

  CoreDataDeviceManager *manager_ = nullptr;
  QStringList mimeTypes_;
  uint32_t actions_ = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

  friend class CoreDataDeviceManager;
};

class CoreDataOffer final : public QtWaylandServer::wl_data_offer {
public:
  CoreDataOffer(CoreDataSource *source, CoreDataDevice *device, wl_client *client,
                int version)
      : QtWaylandServer::wl_data_offer(client, 0, version), source_(source),
        device_(device) {}

  void publish(wl_resource *dataDeviceResource) {
    wl_data_device_send_data_offer(dataDeviceResource, resource()->handle);
    if (!source_)
      return;
    for (const auto &mimeType : source_->mimeTypes())
      send_offer(mimeType);
    if (resource()->version() >= 3) {
      send_source_actions(source_->actions());
      send_action(WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE);
    }
  }

protected:
  void data_offer_accept(Resource *, uint32_t, const QString &mimeType) override {
    if (source_)
      source_->accept(mimeType);
  }

  void data_offer_receive(Resource *, const QString &mimeType, int32_t fd) override {
    if (source_)
      source_->sendData(mimeType, fd);
    else
      ::close(fd);
  }

  void data_offer_destroy(Resource *resource) override {
    wl_resource_destroy(resource->handle);
  }

  void data_offer_finish(Resource *) override {
    if (source_)
      source_->send_dnd_finished();
  }

  void data_offer_set_actions(Resource *resource, uint32_t dndActions,
                              uint32_t preferredAction) override {
    constexpr uint32_t valid =
        WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
        WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE |
        WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    if (dndActions & ~valid) {
      wl_resource_post_error(resource->handle,
                             WL_DATA_OFFER_ERROR_INVALID_ACTION_MASK,
                             "invalid wl_data_offer action mask");
      return;
    }
    const bool preferredValid =
        preferredAction == WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE ||
        preferredAction == WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY ||
        preferredAction == WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE ||
        preferredAction == WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;
    if (!preferredValid ||
        (preferredAction != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE &&
         !(dndActions & preferredAction))) {
      wl_resource_post_error(resource->handle,
                             WL_DATA_OFFER_ERROR_INVALID_ACTION,
                             "invalid wl_data_offer preferred action");
      return;
    }

    if (!source_)
      return;
    const uint32_t available = dndActions & source_->actions();
    uint32_t selected = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;
    if (preferredAction != WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE &&
        (available & preferredAction))
      selected = preferredAction;
    else if (available & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY)
      selected = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
    else if (available & WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE)
      selected = WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE;
    else if (available & WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK)
      selected = WL_DATA_DEVICE_MANAGER_DND_ACTION_ASK;

    send_action(selected);
    source_->send_action(selected);
  }

  void data_offer_destroy_resource(Resource *) override { delete this; }

private:
  QPointer<CoreDataSource> source_;
  CoreDataDevice *device_ = nullptr;
};

class CoreDataDevice final : public QtWaylandServer::wl_data_device {
public:
  CoreDataDevice(CoreDataDeviceManager *manager, QWaylandSeat *seat,
                 wl_client *client, uint32_t id, int version)
      : QtWaylandServer::wl_data_device(client, id, version),
        manager_(manager), seat_(seat) {}

  wl_client *client() const { return resource()->client(); }

  void sendSelection(CoreDataSource *source);

protected:
  void data_device_start_drag(Resource *, wl_resource *sourceResource,
                              wl_resource *, wl_resource *, uint32_t) override {
    // The v3 protocol is accepted so modern clients can use clipboard and
    // negotiate actions safely. LunaDash does not yet expose compositor drag
    // motion targets here; cancel an attempted DnD source instead of leaving
    // the client waiting forever.
    if (auto *source = CoreDataSource::fromResource(sourceResource))
      source->cancel();
  }

  void data_device_set_selection(Resource *, wl_resource *sourceResource,
                                 uint32_t serial) override;

  void data_device_release(Resource *resource) override {
    wl_resource_destroy(resource->handle);
  }

  void data_device_destroy_resource(Resource *) override;

private:
  CoreDataDeviceManager *manager_ = nullptr;
  QPointer<QWaylandSeat> seat_;
};

class CoreDataDeviceManager final : public QtWayland::DataDeviceManager {
public:
  explicit CoreDataDeviceManager(QWaylandCompositor *compositor)
      : QtWayland::DataDeviceManager(compositor), compositor_(compositor) {
    // Qt's constructor just created a v1 global. Remove it before any client can
    // bind and re-use the same generated server object at v3, avoiding duplicate
    // wl_data_device_manager globals in the registry.
    if (m_global) {
      wl_global_destroy(m_global);
      wl_list_remove(&m_displayDestroyedListener.link);
      m_global = nullptr;
    }
    init(compositor->display(), kDataDeviceVersion);

    if (auto *seat = compositor->defaultSeat()) {
      connect(seat, &QWaylandSeat::keyboardFocusChanged, this,
              [this](QWaylandSurface *surface, QWaylandSurface *) {
                focusedClient_ = surface ? surface->waylandClient() : nullptr;
                publishSelection();
              });
    }
  }

  ~CoreDataDeviceManager() override {
    const auto devices = devices_;
    for (auto *device : devices)
      if (device && device->resource())
        wl_resource_destroy(device->resource()->handle);
  }

  void setSelection(CoreDataSource *source, wl_client *owner, uint32_t) {
    if (selection_ == source)
      return;
    if (selection_ && selectionOwner_ != owner)
      selection_->cancel();
    selection_ = source;
    selectionOwner_ = source ? owner : nullptr;
    publishSelection();
  }

  void sourceDestroyed(CoreDataSource *source) {
    if (selection_ != source)
      return;
    selection_ = nullptr;
    selectionOwner_ = nullptr;
    publishSelection();
  }

  void removeDevice(CoreDataDevice *device) { devices_.remove(device); }

protected:
  void data_device_manager_create_data_source(Resource *resource,
                                               uint32_t id) override {
    new CoreDataSource(this, resource->client(), id,
                       std::min(resource->version(), kDataDeviceVersion));
  }

  void data_device_manager_get_data_device(Resource *resource, uint32_t id,
                                           wl_resource *seatResource) override {
    auto *seat = QWaylandSeat::fromSeatResource(seatResource);
    if (!seat) {
      wl_resource_post_error(resource->handle, 0,
                             "wl_data_device_manager received an invalid seat");
      return;
    }
    auto *device =
        new CoreDataDevice(this, seat, resource->client(), id,
                           std::min(resource->version(), kDataDeviceVersion));
    devices_.insert(device);
    if (resource->client() == focusedClient_)
      device->sendSelection(selection_);
  }

private:
  void publishSelection() {
    for (auto *device : devices_)
      if (device && device->client() == focusedClient_)
        device->sendSelection(selection_);
  }

  QPointer<QWaylandCompositor> compositor_;
  QSet<CoreDataDevice *> devices_;
  QPointer<CoreDataSource> selection_;
  wl_client *selectionOwner_ = nullptr;
  wl_client *focusedClient_ = nullptr;
};

void CoreDataSource::notifyDestroyed() {
  if (manager_)
    manager_->sourceDestroyed(this);
  manager_ = nullptr;
}

void CoreDataDevice::sendSelection(CoreDataSource *source) {
  if (!resource())
    return;
  if (!source) {
    send_selection(nullptr);
    return;
  }

  auto *offer =
      new CoreDataOffer(source, this, client(),
                        std::min(resource()->version(), kDataDeviceVersion));
  offer->publish(resource()->handle);
  send_selection(offer->resource()->handle);
}

void CoreDataDevice::data_device_set_selection(Resource *resource,
                                               wl_resource *sourceResource,
                                               uint32_t serial) {
  auto *source = CoreDataSource::fromResource(sourceResource);
  if (sourceResource && !source) {
    wl_resource_post_error(resource->handle, 0,
                           "selection source does not belong to LunaDash");
    return;
  }
  manager_->setSelection(source, resource->client(), serial);
}

void CoreDataDevice::data_device_destroy_resource(Resource *) {
  if (manager_)
    manager_->removeDevice(this);
  manager_ = nullptr;
  delete this;
}

} // namespace

void installCoreDataDeviceV3(QWaylandCompositor *compositor) {
  if (!compositor)
    return;

  auto *d = QWaylandCompositorPrivate::get(compositor);
  if (!d)
    return;

  delete d->data_device_manager;
  d->data_device_manager = new CoreDataDeviceManager(compositor);
  d->retainSelection = false;
  qInfo("LunaDash core protocol: wl_data_device_manager v3");
}

} // namespace LuDash

#else

namespace LuDash {
void installCoreDataDeviceV3(QWaylandCompositor *) {}
} // namespace LuDash

#endif
