#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientWindow.hpp"
#include "compositor/plugins/ExtensionHooks.hpp"
#include "compositor/plugins/PluginManager.hpp"
#include "compositor/renderer/selection/RenderSelection.h"
#include "compositor/session/SessionActions.hpp"
#include "compositor/settings/SettingsApi.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "compositor/wayland/wlroots/WlrootsCompat.hpp"
#include "compositor/wayland/wlroots/WlrootsHeaders.hpp"
#include "compositor/window/WindowTemplate.hpp"
#include "compositor/window/animation/AnimationProfile.hpp"
#include "compositor/xwayland/XWaylandSupport.hpp"
#include "config/BuildConfig.hpp"
#include "config/appearance/AppearancePalette.hpp"
#include "config/appearance/AppearancePresets.hpp"
#include "config/desktop/DesktopPreferenceStore.hpp"
#include "config/desktop/DesktopPreferences.hpp"
#include "config/desktop/PreferenceKeys.hpp"
#include "core/json/JsonAccess.hpp"
#include "core/settings/SettingsTarget.hpp"
#include "core/text/Url.hpp"
#include "desktop/app/DefaultApplications.hpp"
#include "desktop/audio/AudioSettings.hpp"
#include "desktop/display/BrightnessSettings.hpp"
#include "desktop/display/DdcBrightnessSettings.hpp"
#include "desktop/input/InputSettings.hpp"
#include "desktop/network/NetworkStatus.hpp"
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/ShortcutSettings.hpp"
#include "desktop/system/SystemStatus.hpp"
#include "desktop/system/SystemTools.hpp"
#include "desktop/system/UpdateChecker.hpp"
#include "desktop/theme/ThemeSync.hpp"
#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"
#include "desktop/weather/WeatherStatus.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>

namespace LunaDash {
namespace {

Json rectJson(const Rect &rectangle) {
  return Json::object({{"x", rectangle.x()},
                       {"y", rectangle.y()},
                       {"width", rectangle.width()},
                       {"height", rectangle.height()}});
}


Json sessionActionsJson(const SessionActions &actions) {
  const auto snapshot = actions.snapshot();
  Json result = Json::object();
  for (const auto &[action, available] : snapshot.availability)
    result[action] = available;
  result["error"] = snapshot.error;
  return result;
}

Json settingsToolsJson() {
  Json result = Json::array();
  for (const auto &tool : systemSettingsTools())
    result.push_back(Json::object({{"id", tool.id},
                                   {"category", tool.category},
                                   {"name", tool.name},
                                   {"package", tool.package},
                                   {"available", tool.available},
                                   {"configurable", tool.configurable},
                                   {"host", tool.host}}));
  return result;
}

} // namespace

Json WaylandCompositor::describeClients(Json &xwaylandUtilities) const {
  Json entries = Json::array();
  for (const auto &client : clients_) {
    if (client->utility) {
      // Keep notifications out of the task list while exposing their lifetime
      // separately for diagnostics, including the interval before destruction.
      if (client->x11)
        xwaylandUtilities.push_back(
            Json::object({{"id", client->id}, {"mapped", client->mapped}}));
      continue;
    }
    int bufferWidth = 0;
    int bufferHeight = 0;
    if (client->wlSurface) {
      bufferWidth = client->wlSurface->current.width;
      bufferHeight = client->wlSurface->current.height;
    }
    wlr_box contentGeometry{};
    if (client->surface) {
#if WLR_VERSION_MINOR >= 19
      contentGeometry = client->surface->geometry;
#else
      wlr_xdg_surface_get_geometry(client->surface, &contentGeometry);
#endif
    }
    const auto fallback = animationRectangle(client->geometry);
    const auto frame = windowAnimations_
                           ? windowAnimations_->animation().visualGeometry(
                                 client->sceneTree, fallback)
                           : fallback;
    const bool sceneVisible =
        client->sceneTree ? client->sceneTree->node.enabled : false;
    entries.push_back(Json::object({
        {"contentX", contentGeometry.x},
        {"contentY", contentGeometry.y},
        {"contentGeometryWidth",
         client->surface ? contentGeometry.width : bufferWidth},
        {"contentGeometryHeight",
         client->surface ? contentGeometry.height : bufferHeight},
        {"frameX", frame.left},
        {"frameY", frame.top},
        {"frameWidth", frame.width},
        {"frameHeight", frame.height},
        {"cornerRadius", d && d->windowCorners
                             ? d->windowCorners->radius(client->sceneTree)
                             : 0},
        {"floating", client->floating},
        {"contentWidth", bufferWidth},
        {"contentHeight", bufferHeight},
        {"contentVisible", sceneVisible},
        {"contentPaintEnabled", true},
        {"bufferWidth", bufferWidth},
        {"bufferHeight", bufferHeight},
        {"id", client->id},
        {"title", client->title},
        {"appId", client->appId},
        {"icon", client->iconName},
        {"desktop", client->desktop},
        {"x11", client->x11},
        {"workspace", client->workspace},
        {"visible", sceneVisible},
        {"focused", client.get() == focused_},
        {"x", client->geometry.x()},
        {"y", client->geometry.y()},
        {"width", client->geometry.width()},
        {"height", client->geometry.height()},
        {"minimized", client->minimized},
        {"maximized", client->maximized},
        {"fullscreen", client->fullscreen},
        {"hiddenByMaximize", client->hiddenByMaximize},
        {"manualResize", client->manualResize},
        {"mapped", client->mapped}}));
  }
  return entries;
}

Json WaylandCompositor::describeTiling() const {
  const auto snapshot = windowLayout_->snapshot(workspace_);
  Json columns = Json::array();
  Json groups = Json::array();
  std::unordered_set<int> emittedGroups;
  int fallbackGroup = 0;
  for (const auto &column : snapshot.columns) {
    const int group = jsonInt(column.metadata, "group", fallbackGroup++);
    const int row = jsonInt(column.metadata, "row", 0);
    columns.push_back(
        Json::object({{"window", static_cast<std::int64_t>(column.window)},
                      {"width", column.width},
                      {"minimized", column.minimized},
                      {"focused", column.focused},
                      {"columnIndex", group},
                      {"rowIndex", row},
                      {"x", column.geometry.x()},
                      {"y", column.geometry.y()},
                      {"height", column.geometry.height()}}));
    if (emittedGroups.contains(group))
      continue;
    emittedGroups.insert(group);
    auto memberIds = jsonArray(column.metadata, "members");
    if (memberIds.empty())
      memberIds.push_back(static_cast<std::int64_t>(column.window));
    Json members = Json::array();
    bool groupFocused = false;
    for (const auto &idValue : memberIds) {
      const auto id = static_cast<LayoutWindowId>(jsonAsInteger(idValue));
      const auto *member = clientById(static_cast<int>(id));
      if (!member)
        continue;
      const bool memberFocused = member == focused_ && !member->minimized;
      groupFocused = groupFocused || memberFocused;
      members.push_back(Json::object({{"window", static_cast<std::int64_t>(id)},
                                      {"title", member->title},
                                      {"appId", member->appId},
                                      {"icon", member->iconName},
                                      {"focused", memberFocused},
                                      {"minimized", member->minimized}}));
    }
    groups.push_back(Json::object({{"index", group},
                                   {"focused", groupFocused},
                                   {"width", column.width},
                                   {"members", members}}));
  }
  return Json::object(
      {{"focusedWindow", static_cast<std::int64_t>(snapshot.focusedWindow)},
       {"columns", columns},
       {"groups", groups}});
}

Json WaylandCompositor::describeXWayland(Json utilitySurfaces) const {
  const auto snapshot = xwayland_ ? xwayland_->snapshot() : XWaylandSnapshot{};
  return Json{{"available", snapshot.available},
              {"mode", XWaylandSnapshot::mode},
              {"running", snapshot.running},
              {"rootWindowVisible", false},
              {"selectionBridge", snapshot.selectionBridge},
              {"display", snapshot.display},
              {"authority", std::string()},
              {"error", snapshot.error},
              {"utilitySurfaces", std::move(utilitySurfaces)}};
}

Json WaylandCompositor::describeInput(const Json &preferences) const {
  auto *keyboard = d ? d->preferredKeyboard() : nullptr;
  const auto lockEnabled = [keyboard](const char *name) {
    if (!keyboard || !keyboard->keymap)
      return false;
    const auto index = xkb_keymap_mod_get_index(keyboard->keymap, name);
    return index != XKB_MOD_INVALID &&
           (keyboard->modifiers.locked & (xkb_mod_mask_t{1} << index));
  };
  return Json::object(
      {{"layout", keyboardLayoutPreference(
                      preferenceValue(preferences, Preference::keyboardLayout))},
       {"repeatRate", keyboardRepeatRate()},
       {"repeatDelay", keyboardRepeatDelay()},
       {"modifierResends", keyboardDiagnostics_.modifierResends},
       {"keypadKeyForwards", keyboardDiagnostics_.keypadKeyForwards},
       {"lockedModifiers",
        static_cast<std::int64_t>(keyboard ? keyboard->modifiers.locked : 0)},
       {"capsLock", lockEnabled(XKB_MOD_NAME_CAPS)},
       // NumLock is the virtual modifier name; its real modifier varies by keymap.
       {"numLock", lockEnabled("NumLock")},
       {"seatProtocolVersion", WlrootsCompat::expectedSeatProtocolVersion()},
       {"dataDeviceProtocolVersion", WlrootsCompat::dataDeviceProtocolVersion},
       {"backend", "wlroots"},
       {"qtInput", false},
       {"inputMethodBridge", d ? d->inputBridgeReady() : false}});
}

Json WaylandCompositor::describeWallpaperState() const {
  // Every status poll keeps the video wallpaper player in step with the
  // wallpaper, whichever tool changed it.
  const auto path = wallpaperImagePath();
  videoWallpaper_->sync(path, clientEnvironment_);
  const bool video = wallpaperIsVideo(path);
  std::string posterUri;
  if (video) {
    std::error_code posterError;
    const auto poster = wallpaperPoster(path);
    if (std::filesystem::is_regular_file(poster, posterError))
      posterUri = localFileUri(poster.string());
  }
  // A player that is still being stopped no longer counts.
  bool live = false;
  if (video)
    for (const auto *layer : d->layers)
      live = live || (layer->mapped && videoWallpaperSurface(layer->surface));
  return Json::object({{"wallpaperImage", path.empty() ? std::string{}
                                                       : localFileUri(path)},
                       {"wallpaperPoster", posterUri},
                       {"wallpaperLive", live},
                       {"wallpaperVideoError", videoWallpaper_->error()}});
}

Json WaylandCompositor::state() const {
  const auto wallpaper = describeWallpaperState();
  Json xwaylandUtilities = Json::array();
  const auto clients = describeClients(xwaylandUtilities);

  auto preferences = desktopPreferences();
  const auto palette = appearancePalette(preferences);
  const auto extensions = pluginManager_->snapshot();
  preferences["plugins"] = jsonArray(extensions, "installed");
  const auto layoutSettings = currentWindowLayoutSettings();
  const auto layoutTarget = Settings::target(
      "layout:" + windowTemplate_->key, "Window layout", "layout", "Windows",
      windowTemplate_->layoutSettingsSchema, layoutSettings);
  const auto area = workArea();
  auto *glass = d ? d->windowGlass.get() : nullptr;

  Json result{
      {"version", std::string(BuildConfig::version)},
      {"settingsApi",
       Json::object({{"version", 1},
                     {"targets", settingsApiTargets(extensions, layoutTarget)}})},
      {"layoutMode", windowTemplate_ ? windowTemplate_->layoutMode : std::string()},
      // The active variant, or the configured one while another template is in use.
      {"tilingMode",
       windowTemplate_ &&
               windowTemplate_->variantPreference == Preference::tilingMode.name
           ? windowTemplate_->variant
           : preferenceValue(preferences, Preference::tilingMode)},
      {"windowLayout",
       Json::object({
           {"template", windowTemplate_ ? windowTemplate_->key : std::string()},
           {"allowOverlap",
            windowTemplate_ ? windowTemplate_->allowOverlap : false},
           {"schema", windowTemplate_ ? windowTemplate_->layoutSettingsSchema
                                      : Json::object()},
           {"values", layoutSettings},
           {"actions",
            windowTemplate_ ? windowTemplate_->layoutActions : Json::object()}})},
      {"defaultApps", defaultApplications()},
      {"extensions", extensions},
      {"shellAnimationDuration",
       shellAnimationDuration(*pluginManager_,
                              preferenceValue(preferences,
                                              Preference::animationDuration))},
      {"workArea", rectJson(area)},
      {"audio", audioSettings_->snapshot()},
      {"power", powerSettings_->snapshot()},
      {"sessionActions", sessionActionsJson(*sessionActions_)},
      {"shortcuts", shortcutSettings_->snapshot()},
      {"update", updateChecker_->snapshot()},
      {"settingsTools", settingsToolsJson()},
      {"display", d ? d->displaySnapshot() : Json::object()},
      {"settingsSerial", shellRequests_.settingsSerial},
      {"settingsPage", shellRequests_.settingsPage},
      {"pickerSerial", shellRequests_.pickerSerial},
      {"launcherSerial", shellRequests_.launcherSerial},
      {"launcherOpen", shellRequests_.launcherVisible},
      {"input", describeInput(preferences)},
      {"blurReady", glass && glass->ready()},
      {"blurFailed", glass && glass->failed()},
      {"blurFrames", static_cast<std::int64_t>(glass ? glass->frames() : 0)},
      {"blurError", glass ? glass->error() : std::string()},
      {"session", session_.describe()},
      {"activeAnimations",
       windowAnimations_ ? windowAnimations_->activeCount() : 0},
      {"overview", Json::object({{"open", overview_.open},
                                 {"progress", overview_.progress},
                                 {"previews", static_cast<std::int64_t>(
                                                  overview_.previews.size())}})},
      {"xwayland", describeXWayland(xwaylandUtilities)},
      {"screenCapture", Json::object({{"protocol", ScreenCapture::protocol},
                                      {"version", ScreenCapture::protocolVersion},
                                      {"frames", 0},
                                      {"lastCapture", capture_.last},
                                      {"error", capture_.error},
                                      {"busy", screenCapture_->busy()},
                                      {"phase", screenCapture_->phase()}})},
      {"activationEnvironment",
       Json::object({{"published", activationEnvironment_.published},
                     {"error", activationEnvironment_.error}})},
      {"brightness", brightnessSettings_->snapshot()},
      {"ddcBrightness", ddcBrightnessSettings_->snapshot()},
      {"appearance", preferences},
      {"palette", palette},
      {"applicationTheme", synchronizeApplicationTheme(preferences, palette)},
      {"appearancePresets", appearancePresets()},
      {"wallpapers", wallpaperSnapshot()},
      {"shellAction", shellRequests_.action},
      {"shellActionSerial", shellRequests_.actionSerial},
      {"scratchpad", Json::object({{"window", scratchpad_.window},
                                   {"pending", scratchpad_.pending},
                                   {"error", scratchpad_.error}})},
      {"setupComplete", setupComplete()},
      {"network", networkStatus_->snapshot()},
      {"system", systemStatus_->snapshot()},
      {"weather", weatherStatus_->snapshot()},
      {"workspace", workspace_},
      {"workspaceCount", workspaceCount()},
      {"processFailure", lifecycle_.processFailure},
      {"clients", clients},
      {"pointerDrag", Json::object({{"window", d ? d->pointerWindow : 0},
                                    {"target", d ? d->pointerTarget : 0},
                                    {"edge", d ? d->pointerEdge : 0}})},
      {"tiling", describeTiling()},
      {"layerSurfaces", d ? d->mappedLayerCount() : 0},
      {"xdgPopupCount",
       d ? static_cast<std::int64_t>(d->xdgPopups.size()) : std::int64_t{0}},
      {"wallpaper", loadWallpaperSettings().value("shader", 0)},
      {"shutdown", lifecycle_.testStopping},
      {"graphicsApi", "wlroots"},
      {"renderer", std::string(ludash_renderer_name(d ? d->renderer : nullptr))},
      {"vulkanAvailable", ludash_renderer_vulkan_available()},
      {"shaderReady", d && d->renderer},
      {"graphicsFailed", lifecycle_.processFailure},
      {"graphicsMajor", 0},
      {"graphicsMinor", 0}};
  result.update(wallpaper);
  return result;
}

} // namespace LunaDash
