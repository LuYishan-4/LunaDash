#include "core/templates/Module.hpp"
#include "shell/runtime/ShellTool.hpp"

int main(int argc, char **argv) {
  return LunaDash::Templates::runModule<LunaDash::ShellTool>(argc, argv);
}
