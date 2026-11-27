#include "compositor/capture/ScreenCapture.hpp"
#include "compositor/client/ClientSurface.hpp"
#include "compositor/client/ClientWindowControl.hpp"
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
#include "desktop/power/PowerSettings.hpp"
#include "desktop/shortcuts/KeyBindings.hpp"
#include "desktop/system/SystemTools.hpp"
#include "desktop/theme/ThemeSync.hpp"
#include "desktop/wallpaper/VideoWallpaperPlayer.hpp"
#include "desktop/wallpaper/WallpaperPalette.hpp"
#include "desktop/wallpaper/WallpaperSettings.hpp"

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

Json WaylandCompositor::describeClient(const ClientWindow &client) const {
  const Rect geometry = columnGeometry(client);
  return Json::object({{"id", client.id},
                       {"title", client.title},
                       {"appId", client.appId},
                       {"icon", client.iconName},
                       {"iconFile", client.iconFile},
                       {"x11", clientIsX11(client)},
                       {"workspace", client.state.workspace},
                       {"focused", &client == focused_},
                       {"mapped", client.state.mapped},
                       {"minimized", client.state.minimized},
                       {"maximized", client.state.maximized},
                       {"fullscreen", client.state.fullscreen},
                       {"floating", client.state.floating},

                       {"x", geometry.x()},
                       {"y", geometry.y()},
                       {"width", geometry.width()},
                       {"height", geometry.height()}});
}

Json WaylandCompositor::describeClients(Json &xwaylandUtilities) const {
  Json entries = Json::array();
  for (const auto &client : clients_) {
    if (client->utility) {
      // Keep notifications out of the task list while exposing their lifetime
      // separately for diagnostics, including the interval before destruction.
      if (clientIsX11(*client))
        xwaylandUtilities.push_back(Json::object(
            {{"id", client->id}, {"mapped", client->state.mapped}}));
      continue;
    }
    auto *surface = clientSurface(*client);
    const int bufferWidth = surface ? surface->current.width : 0;
    const int bufferHeight = surface ? surface->current.height : 0;
    wlr_box contentGeometry{};
    if (auto *xdg = clientXdg(*client)) {
#if WLR_VERSION_MINOR >= 19
      contentGeometry = xdg->geometry;
#else
      wlr_xdg_surface_get_geometry(xdg, &contentGeometry);
#endif
    } else {
      contentGeometry.width = bufferWidth;
      contentGeometry.height = bufferHeight;
    }
    // The window frame and its client surface have the same geometry.
    const auto fallback = animationRectangle(client->layout.geometry);
    auto frame = windowAnimations_
                     ? windowAnimations_->animation().visualGeometry(
                           client->scene.tree, fallback)
                     : fallback;
    auto entry = describeClient(*client);
    entry.update(Json::object(
        {{"frameX", frame.left},
         {"frameY", frame.top},
         {"frameWidth", frame.width},
         {"frameHeight", frame.height},
         {"contentX", contentGeometry.x},
         {"contentY", contentGeometry.y},
         {"contentGeometryWidth", contentGeometry.width},
         {"contentGeometryHeight", contentGeometry.height},
         {"bufferWidth", bufferWidth},
         {"bufferHeight", bufferHeight},
         {"cornerRadius", d && d->windowCorners
                              ? d->windowCorners->radius(client->scene.tree)
                              : 0},
         {"visible", client->scene.tree && client->scene.tree->node.enabled},
         {"hiddenByMaximize", client->layout.hiddenByMaximize},
         {"manualResize", client->layout.manualResize}}));
    entries.push_back(std::move(entry));
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
      const bool memberFocused = member == focused_ && !member->state.minimized;
      groupFocused = groupFocused || memberFocused;
      members.push_back(Json::object({{"window", static_cast<std::int64_t>(id)},
                                      {"title", member->title},
                                      {"appId", member->appId},
                                      {"icon", member->iconName},
                                      {"focused", memberFocused},
                                      {"minimized", member->state.minimized}}));
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

Json WaylandCompositor::describeVersion() const {
  return Json::object(
      {{"version", std::string(BuildConfig::version)},
       {"graphicsApi", "wlroots"},
       {"renderer",
        std::string(ludash_renderer_name(d ? d->renderer : nullptr))},
       {"vulkanAvailable", ludash_renderer_vulkan_available()}});
}

Json WaylandCompositor::describeWorkspaces() const {
  return Json::object(
      {{"workspace", workspace_}, {"workspaceCount", workspaceCount()}});
}

Json WaylandCompositor::describeLayout() const {
  return Json::object(
      {{"layoutMode",
        windowTemplate_ ? windowTemplate_->layoutMode : std::string()},
       {"windowLayout",
        Json::object(
            {{"template",
              windowTemplate_ ? windowTemplate_->key : std::string()},
             {"allowOverlap",
              windowTemplate_ ? windowTemplate_->allowOverlap : false},
             {"schema", windowTemplate_ ? windowTemplate_->layoutSettingsSchema
                                        : Json::object()},
             {"values", currentWindowLayoutSettings()},
             {"actions", windowTemplate_ ? windowTemplate_->layoutActions
                                         : Json::object()}})},
       {"workArea", rectJson(workArea())},
       {"tiling", describeTiling()},
       {"activeAnimations",
        windowAnimations_ ? windowAnimations_->activeCount() : 0},
       {"overview",
        Json::object({{"open", overview_.open},
                      {"progress", overview_.progress},
                      {"previews",
                       static_cast<std::int64_t>(overview_.previews.size())}})},
       {"pointerDrag", Json::object({{"window", d ? d->pointerWindow : 0},
                                     {"target", d ? d->pointerTarget : 0},
                                     {"edge", d ? d->pointerEdge : 0}})}});
}

Json WaylandCompositor::describePreferences() const {
  auto preferences = desktopPreferences();
  preferences["plugins"] = jsonArray(pluginManager_->snapshot(), "installed");
  return preferences;
}

Json WaylandCompositor::describeAppearance() const {
  const auto preferences = describePreferences();
  const auto palette = appearancePalette(preferences);
  return Json::object(
      {{"appearance", preferences},
       {"palette", palette},
       {"applicationTheme", synchronizeApplicationTheme(preferences, palette)},
       {"appearancePresets", appearancePresets()}});
}

Json WaylandCompositor::describeWallpaper() const {
  auto result = describeWallpaperState();
  result["wallpapers"] = wallpaperSnapshot();
  result["shader"] = loadWallpaperSettings().value("shader", 0);
  return result;
}

Json WaylandCompositor::describeDevices() const {
  return Json::object({{"audio", audioSettings_->snapshot()},
                       {"power", powerSettings_->snapshot()},
                       {"brightness", brightnessSettings_->snapshot()},
                       {"ddcBrightness", ddcBrightnessSettings_->snapshot()}});
}

Json WaylandCompositor::describeSession() const {
  Json xwaylandUtilities = Json::array();
  describeClients(xwaylandUtilities);
  return Json::object(
      {{"session", session_.describe()},
       {"sessionActions", sessionActionsJson(*sessionActions_)},
       {"setupComplete", setupComplete()},
       {"shutdown", lifecycle_.testStopping},
       {"processFailure", lifecycle_.processFailure},
       {"activationEnvironment",
        Json::object({{"published", activationEnvironment_.published},
                      {"error", activationEnvironment_.error}})},
       {"launcherOpen", launcherVisible_},
       {"defaultApps", defaultApplications()},
       {"settingsTools", settingsToolsJson()},
       {"screenCapture",
        Json::object({{"protocol", ScreenCapture::protocol},
                      {"version", ScreenCapture::protocolVersion},
                      {"lastCapture", capture_.last},
                      {"error", capture_.error},
                      {"busy", screenCapture_->busy()},
                      {"phase", screenCapture_->phase()}})},
       {"xwayland", describeXWayland(std::move(xwaylandUtilities))}});
}

Json WaylandCompositor::describeRenderer() const {
  auto *glass = d ? d->windowGlass.get() : nullptr;
  return Json::object(
      {{"shaderReady", d && d->renderer},
       {"blur",
        Json::object(
            {{"ready", glass && glass->ready()},
             {"failed", glass && glass->failed()},
             {"frames", static_cast<std::int64_t>(glass ? glass->frames() : 0)},
             {"error", glass ? glass->error() : std::string()},
             {"cause", glass ? glass->cause() : std::string()}})},
       {"layerSurfaces", d ? d->mappedLayerCount() : 0},
       {"xdgPopupCount",
        d ? static_cast<std::int64_t>(d->xdgPopups.size()) : std::int64_t{0}}});
}

Json WaylandCompositor::describeExtensions() const {
  return Json::object(
      {{"extensions", pluginManager_->snapshot()},
       {"shellAnimationDuration",
        shellAnimationDuration(
            *pluginManager_, preferenceValue(desktopPreferences(),
                                             Preference::animationDuration))}});
}

Json WaylandCompositor::describeSettings() const {
  const auto layoutTarget = Settings::target(
      "layout:" + windowTemplate_->key, "Window layout", "layout", "Windows",
      windowTemplate_->layoutSettingsSchema, currentWindowLayoutSettings());
  return Json::object(
      {{"version", 1},
       {"targets",
        settingsApiTargets(pluginManager_->snapshot(), layoutTarget)}});
}

} // namespace LunaDash
