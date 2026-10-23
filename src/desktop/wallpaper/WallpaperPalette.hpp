#pragma once
#include <nlohmann/json.hpp>

#include <filesystem>

struct wl_event_loop;

namespace LunaDash {
void refreshWallpaperPalette(wl_event_loop *eventLoop,
                             const std::filesystem::path &path, bool video);
void cancelWallpaperPalette();
std::filesystem::path wallpaperPoster(const std::filesystem::path &path);
std::filesystem::path
wallpaperPlaybackPath(const std::filesystem::path &path);
nlohmann::json wallpaperMediaStatus();
} // namespace LunaDash
