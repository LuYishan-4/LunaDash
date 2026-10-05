#include "shell/runtime/ShellRenderer.hpp"
namespace LunaDash {
bool configureShellRendering(QProcessEnvironment &environment,
                             bool nvidiaDriver) {
  const auto requested = environment.value("LUDASH_SHELL_RENDERER", "auto");
  if (requested != "auto" && requested != "opengl" && requested != "vulkan" &&
      requested != "software")
    return false;
  // Preserve explicit Qt choices, including a software backend in headless
  // sessions. This policy is applied only to the shell's child environment.
  if (requested == "auto" &&
      (!environment.value("QT_QUICK_BACKEND").isEmpty() ||
       !environment.value("QSG_RHI_BACKEND").isEmpty()))
    return true;
  // Qt's EGLStream path can accumulate NVIDIA sync_file descriptors. Limit
  // the fallback to the shell; the compositor still renders GL/GLES effects.
  const bool software =
      requested == "software" || (requested == "auto" && nvidiaDriver);
  const auto api = requested == "vulkan" ? "vulkan" : "opengl";
  environment.insert("LUDASH_SHELL_RENDERER", software ? "software" : api);
  if (software) {
    environment.insert("QT_QUICK_BACKEND", "software");
    environment.remove("QSG_RHI_BACKEND");
  } else {
    environment.remove("QT_QUICK_BACKEND");
    environment.insert("QSG_RHI_BACKEND", api);
  }
  return true;
}
} // namespace LunaDash
