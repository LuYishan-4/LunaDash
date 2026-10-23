#include "shell/runtime/ShellRenderer.hpp"
namespace LunaDash {
bool configureShellRendering(ShellRenderingEnvironment &environment,
                             bool nvidiaDriver) {
  const auto requestedEntry = environment.find("LUDASH_SHELL_RENDERER");
  const auto requested = requestedEntry == environment.end()
                             ? std::string("auto")
                             : requestedEntry->second;
  if (requested != "auto" && requested != "opengl" && requested != "vulkan" &&
      requested != "software")
    return false;
  // Preserve explicit Qt choices, including a software backend in headless
  // sessions. This policy is applied only to the shell's child environment.
  const auto quickRenderer = environment.find("QT_QUICK_BACKEND");
  const auto rhiRenderer = environment.find("QSG_RHI_BACKEND");
  if (requested == "auto" &&
      ((quickRenderer != environment.end() && !quickRenderer->second.empty()) ||
       (rhiRenderer != environment.end() && !rhiRenderer->second.empty())))
    return true;
  // Qt's EGLStream path can accumulate NVIDIA sync_file descriptors. Limit
  // the fallback to the shell; the compositor still renders GL/GLES effects.
  const bool software =
      requested == "software" || (requested == "auto" && nvidiaDriver);
  const std::string api = requested == "vulkan" ? "vulkan" : "opengl";
  environment["LUDASH_SHELL_RENDERER"] =
      software ? std::string("software") : api;
  if (software) {
    environment["QT_QUICK_BACKEND"] = "software";
    environment.erase("QSG_RHI_BACKEND");
  } else {
    environment.erase("QT_QUICK_BACKEND");
    environment["QSG_RHI_BACKEND"] = api;
  }
  return true;
}
} // namespace LunaDash
