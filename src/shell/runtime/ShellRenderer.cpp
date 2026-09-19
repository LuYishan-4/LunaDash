#include "shell/runtime/ShellRenderer.hpp"
namespace LunaDash {
bool configureShellRendering(QProcessEnvironment &environment,
                             bool nvidiaDriver) {
  const auto requested = environment.value("LUDASH_SHELL_RENDERER", "auto");
  if (requested != "auto" && requested != "opengl" && requested != "software")
    return false;
  // Qt's EGLStream path can accumulate NVIDIA sync_file descriptors. Limit
  // the fallback to the shell; the compositor still renders GL/GLES effects.
  const bool software =
      requested == "software" || (requested == "auto" && nvidiaDriver);
  environment.insert("LUDASH_SHELL_RENDERER", software ? "software" : "opengl");
  if (software) {
    environment.insert("QT_QUICK_BACKEND", "software");
    environment.remove("QSG_RHI_BACKEND");
  } else {
    environment.remove("QT_QUICK_BACKEND");
    environment.insert("QSG_RHI_BACKEND", "opengl");
  }
  return true;
}
} // namespace LunaDash
