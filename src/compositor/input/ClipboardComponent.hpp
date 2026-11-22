#pragma once

#include "compositor/wayland/Register.hpp"

#include <memory>
#include <vector>

namespace LunaDash {

class WaylandCompositor::Impl::ClipboardComponent final
    : public Component,
      public LunaClint<ClipboardComponent> {
public:
  explicit ClipboardComponent(Impl *owner);
  ~ClipboardComponent() override;
  bool start() override;

private:
  struct Capture;
  struct ReadTransfer;
  struct WriteTransfer;

  void onRequestSelection(wlr_seat_request_set_selection_event *event);
  void onSelection();
  void onSourceDestroy();
  void onCaptureTimeout();
  void onCaptureReadable(ReadTransfer *transfer, uint32_t mask);
  void finishCapture();
  void cancelCapture();
  void sendStoredSelection(const char *mimeType, int fd);
  void onWriteReady(WriteTransfer *transfer, uint32_t mask);
  void finishWrite(WriteTransfer *transfer);

  Slot requestSelection;
  Slot selection;
  std::unique_ptr<Capture> capture_;
  std::vector<std::unique_ptr<WriteTransfer>> writes_;
  wlr_data_source *ownedSource_ = nullptr;
};

} // namespace LunaDash
