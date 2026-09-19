#include "compositor/wayland/Runtime.hpp"
#include "compositor/wayland/SurfaceText.hpp"
#include <QJsonArray>
#include <QSettings>
#include <QTimer>
#include <cmath>

namespace LunaDash {
namespace {
QString modeId(int width, int height, int refresh) {
  return QString("%1x%2@%3").arg(width).arg(height).arg(refresh);
}
} // namespace

QJsonArray WaylandCompositor::Impl::displayModes() const {
  QJsonArray result;
  if (!primaryOutput)
    return result;
  wlr_output_mode *mode = nullptr;
  wl_list_for_each(mode, &primaryOutput->modes, link) {
    result.append(
        QJsonObject{{"id", modeId(mode->width, mode->height, mode->refresh)},
                    {"label", QString("%1 × %2 · %3 Hz")
                                  .arg(mode->width)
                                  .arg(mode->height)
                                  .arg(mode->refresh / 1000.0, 0, 'f', 2)}});
  }
  if (result.isEmpty() && nested && !fullscreen) {
    for (const QSize size :
         {QSize(1280, 720), QSize(1440, 900), QSize(1920, 1080)})
      result.append(QJsonObject{
          {"id", modeId(size.width(), size.height(), 0)},
          {"label", QString("%1 × %2").arg(size.width()).arg(size.height())}});
  }
  return result;
}

bool WaylandCompositor::Impl::configureDisplay(const QJsonObject &changes,
                                               QString *error) {
  if (!primaryOutput || pendingDisplay || changes.isEmpty()) {
    *error =
        "Confirm or revert the pending display change before applying another.";
    return false;
  }
  for (auto it = changes.begin(); it != changes.end(); ++it) {
    if (it.key() != "mode" && it.key() != "scale") {
      *error = "Unknown display setting.";
      return false;
    }
  }
  const double scale = changes.value("scale").toDouble(primaryOutput->scale);
  if ((changes.contains("scale") && !changes.value("scale").isDouble()) ||
      !std::isfinite(scale) || scale < 1.0 || scale > 3.0) {
    *error = "Display scale must be between 1 and 3.";
    return false;
  }
  wlr_output_state next;
  wlr_output_state_init(&next);
  wlr_output_state_set_scale(&next, static_cast<float>(scale));
  if (changes.contains("mode")) {
    const QString id = changes.value("mode").toString();
    bool found = false;
    wlr_output_mode *mode = nullptr;
    wl_list_for_each(mode, &primaryOutput->modes, link) {
      if (modeId(mode->width, mode->height, mode->refresh) == id) {
        wlr_output_state_set_mode(&next, mode);
        found = true;
        break;
      }
    }
    if (!found && nested && !fullscreen) {
      for (const auto &entry : displayModes()) {
        if (entry.toObject().value("id").toString() != id)
          continue;
        const auto size = id.section('@', 0, 0).split('x');
        wlr_output_state_set_custom_mode(&next, size[0].toInt(),
                                         size[1].toInt(), 0);
        found = true;
        break;
      }
    }
    if (!found) {
      wlr_output_state_finish(&next);
      *error = "Choose a mode advertised by the active display.";
      return false;
    }
  }
  wlr_output_state_init(&previousDisplay);
  if (primaryOutput->current_mode)
    wlr_output_state_set_mode(&previousDisplay, primaryOutput->current_mode);
  else
    wlr_output_state_set_custom_mode(&previousDisplay, primaryOutput->width,
                                     primaryOutput->height,
                                     primaryOutput->refresh);
  wlr_output_state_set_scale(&previousDisplay, primaryOutput->scale);
  if (!wlr_output_test_state(primaryOutput, &next) ||
      !wlr_output_commit_state(primaryOutput, &next)) {
    wlr_output_state_finish(&previousDisplay);
    wlr_output_state_finish(&next);
    *error = "The display rejected this resolution, refresh rate or scale.";
    return false;
  }
  wlr_output_state_finish(&next);
  pendingDisplay = primaryOutput;
  displayError.clear();
  if (!displayRevertTimer) {
    displayRevertTimer = new QTimer(q);
    displayRevertTimer->setSingleShot(true);
    QObject::connect(displayRevertTimer, &QTimer::timeout, q,
                     [this] { revertDisplay(); });
  }
  displayRevertTimer->start(15000);
  updateBackground();
  arrangeLayers();
  q->arrange();
  return true;
}

void WaylandCompositor::Impl::confirmDisplay() {
  if (!pendingDisplay)
    return;
  QSettings settings;
  const QString prefix = "display/" + safeUtf8(pendingDisplay->name) + "/";
  settings.setValue(prefix + "width", pendingDisplay->width);
  settings.setValue(prefix + "height", pendingDisplay->height);
  settings.setValue(prefix + "refresh", pendingDisplay->refresh);
  settings.setValue(prefix + "scale", pendingDisplay->scale);
  clearPendingDisplay();
}

void WaylandCompositor::Impl::clearPendingDisplay() {
  if (displayRevertTimer)
    displayRevertTimer->stop();
  if (pendingDisplay)
    wlr_output_state_finish(&previousDisplay);
  pendingDisplay = nullptr;
}

void WaylandCompositor::Impl::revertDisplay() {
  if (!pendingDisplay)
    return;
  if (!wlr_output_commit_state(pendingDisplay, &previousDisplay))
    displayError = "Could not restore the previous display mode.";
  clearPendingDisplay();
  updateBackground();
  arrangeLayers();
  q->arrange();
}
} // namespace LunaDash
