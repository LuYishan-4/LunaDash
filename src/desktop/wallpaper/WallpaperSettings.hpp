#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

namespace LunaDash {
std::filesystem::path wallpaperImagePath();
nlohmann::json wallpaperLibrarySnapshot();
nlohmann::json wallpaperSnapshot();
bool wallpaperIsVideo(const std::filesystem::path &path);
bool refreshWallpaperLibrary(std::string *error = nullptr);
bool createWallpaperDirectory(std::string *error);
bool setWallpaperFavorite(const std::filesystem::path &path, bool favorite,
                          std::string *error);
bool selectRandomWallpaper(std::string *error);
std::uint64_t wallpaperRevision();
bool setWallpaperImage(const std::filesystem::path &path,
                       std::string *error = nullptr);
bool resetWallpaperImage(std::string *error = nullptr);
} // namespace LunaDash
