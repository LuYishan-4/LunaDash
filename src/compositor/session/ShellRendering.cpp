#include "compositor/session/ShellRendering.hpp"
namespace LunaDash {
bool configureSlintLuRendering(ShellRenderingEnvironment &environment) {
  const auto requestedEntry = environment.find("LUDASH_SHELL_RENDERER");
  const auto requested = requestedEntry == environment.end()
                             ? std::string("auto")
                             : requestedEntry->second;
  if (requested != "auto" && requested != "opengl" && requested != "vulkan" &&
      requested != "software")
    return false;
  if (requested != "auto")
    environment["SLINTLU_RENDERER"] = requested;
  return true;
}
} // namespace LunaDash
