#pragma once

#include "compositor/commands/CommandRegistry.hpp"

namespace LunaDash {

// The compositor's built-in control methods, registered by domain. Each
// register function lives in the file of its domain.
struct CompositorCommands {
  static void registerBuiltins(CommandRegistry &registry);

  static void registerSessionCommands(CommandRegistry &registry);    // SessionCommands.cpp
  static void registerCaptureCommands(CommandRegistry &registry);    // CaptureCommands.cpp
  static void registerWindowCommands(CommandRegistry &registry);     // WindowCommands.cpp
  static void registerLaunchCommands(CommandRegistry &registry);     // LaunchCommands.cpp
  static void registerSettingsCommands(CommandRegistry &registry);   // SettingsCommands.cpp
  static void registerWallpaperCommands(CommandRegistry &registry);  // WallpaperCommands.cpp
  static void registerDeviceCommands(CommandRegistry &registry);     // DeviceCommands.cpp
  static void registerExtensionCommands(CommandRegistry &registry);  // ExtensionCommands.cpp
};

} // namespace LunaDash
