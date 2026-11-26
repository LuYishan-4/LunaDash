#include "compositor/client/ClientWindow.hpp"
#include "compositor/ipc/ControlServer.hpp"
#include "compositor/wayland/Register.hpp"
#include "compositor/wayland/WaylandCompositor.hpp"
#include "core/json/JsonAccess.hpp"
#include "desktop/shortcuts/KeyBindings.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace LunaDash {
namespace {

constexpr std::array<std::string_view, 6> kEvents{"workspace",   "window",
                                                  "appearance",  "wallpaper",
                                                  "keybindings", "shutdown"};

std::string reply(bool success, const std::string &error = {}) {
  Json result{{"success", success}};
  if (!error.empty())
    result["error"] = error;
  return jsonCompact(result);
}

std::string line(std::string_view event, Json body) {
  body["event"] = event;
  return jsonCompact(body);
}

std::vector<std::string> windowChanges(const Json &old, const Json &now) {
  std::vector<std::string> changes;
  const auto changed = [&](const char *key) {
    return jsonValue(old, key) != jsonValue(now, key);
  };
  const auto turned = [&](const char *key, const char *on, const char *off) {
    if (changed(key))
      changes.push_back(jsonBool(now, key) ? on : off);
  };
  if (changed("focused") && jsonBool(now, "focused"))
    changes.push_back("focus");
  if (changed("title") || changed("appId") || changed("icon"))
    changes.push_back("title");

  if (changed("workspace") || changed("x") || changed("y"))
    changes.push_back("move");
  turned("mapped", "map", "unmap");
  turned("minimized", "minimize", "restore");
  turned("maximized", "maximize", "restore");
  if (changed("fullscreen"))
    changes.push_back("fullscreen_mode");
  if (changed("floating"))
    changes.push_back("floating");
  std::ranges::sort(changes);
  changes.erase(std::ranges::unique(changes).begin(), changes.end());
  return changes;
}

// The lines that take a subscriber of `event` from `old` to `now`.
std::vector<std::string> eventLines(std::string_view event, const Json &old,
                                    const Json &now) {
  std::vector<std::string> lines;
  if (event == "workspace") {
    const bool focus =
        jsonValue(old, "workspace") != jsonValue(now, "workspace");
    lines.push_back(
        line(event, {{"change", focus ? "focus" : "count"},
                     {"current", jsonValue(now, "workspace")},
                     {"old", jsonValue(old, "workspace")},
                     {"workspaceCount", jsonValue(now, "workspaceCount")}}));
  } else if (event == "window") {
    for (auto it = now.begin(); it != now.end(); ++it) {
      if (!old.contains(it.key())) {
        lines.push_back(
            line(event, {{"change", "new"}, {"container", it.value()}}));
        continue;
      }
      for (const auto &change : windowChanges(old[it.key()], it.value()))
        lines.push_back(
            line(event, {{"change", change}, {"container", it.value()}}));
    }
    for (auto it = old.begin(); it != old.end(); ++it)
      if (!now.contains(it.key()))
        lines.push_back(
            line(event, {{"change", "close"}, {"container", it.value()}}));
  } else if (event == "shutdown") {
    if (jsonAsBool(now))
      lines.push_back(line(event, {{"change", "exit"}}));
  } else {
    lines.push_back(
        line(event, {{"change", "update"}, {std::string(event), now}}));
  }
  return lines;
}

} // namespace

Json WaylandCompositor::eventSnapshot(std::string_view event) const {
  if (event == "workspace")
    return describeWorkspaces();
  if (event == "window") {
    Json windows = Json::object();
    for (const auto &client : clients_)
      if (!client->utility)
        windows[std::to_string(client->id)] = describeClient(*client);
    return windows;
  }
  if (event == "appearance")
    return describePreferences();
  if (event == "wallpaper")
    return describeWallpaper();
  if (event == "keybindings")
    return keyBindings_->snapshot();
  return lifecycle_.testStopping;
}

std::optional<ControlServer::Subscription>
WaylandCompositor::subscribe(std::string_view request) {
  const auto document = parseJsonObject(request);
  if (!document.is_object() || jsonString(document, "method") != "subscribe")
    return std::nullopt;
  const auto names = parseJson(jsonString(document, "value"));
  if (!names.is_array() || names.empty())
    return ControlServer::Subscription{
        reply(false, "Expected a JSON array of event names"), {}};
  std::vector<std::string> events;
  for (const auto &name : names) {
    if (!name.is_string() ||
        std::ranges::find(kEvents, name.get<std::string>()) == kEvents.end())
      return ControlServer::Subscription{
          reply(false, "Unknown event: " + jsonCompact(name)), {}};
    if (std::ranges::find(events, name.get<std::string>()) == events.end())
      events.push_back(name.get<std::string>());
  }
  // Subscribers already there hear what changed first; the new one starts
  // from now and asks the get-* requests for the state itself.
  publishEvents();
  for (const auto &event : events)
    publishedEvents_[event] = eventSnapshot(event);
  return ControlServer::Subscription{reply(true), std::move(events)};
}

bool WaylandCompositor::hasSubscribers() const {
  return controlServer_ &&
         std::ranges::any_of(kEvents, [this](std::string_view event) {
           return controlServer_->subscribed(event);
         });
}

void WaylandCompositor::publishEvents() {
  if (!controlServer_)
    return;
  for (const auto event : kEvents) {
    if (!controlServer_->subscribed(event))
      continue;
    auto now = eventSnapshot(event);
    auto &old = publishedEvents_[std::string(event)];
    if (now == old)
      continue;
    for (const auto &text : eventLines(event, old, now))
      controlServer_->broadcast(event, text);
    old = std::move(now);
  }
}

void WaylandCompositor::scheduleEvents() {
  if (eventsScheduled_ || !d || !hasSubscribers())
    return;
  eventsScheduled_ = true;
  d->scheduleCallback(0, [this] {
    eventsScheduled_ = false;
    publishEvents();
  });
}

// Changes that pass neither arrange() nor a command (a palette finished in the
// background, a preferences file edited by hand) are found by a slow check.
void WaylandCompositor::watchEvents() {
  if (!d)
    return;
  d->scheduleCallback(2000, [this] {
    if (lifecycle_.shuttingDown)
      return;
    scheduleEvents();
    watchEvents();
  });
}

} // namespace LunaDash
