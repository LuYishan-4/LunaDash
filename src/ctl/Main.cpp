#include "core/templates/Module.hpp"
#include "ctl/command/ControlClient.hpp"

int main(int argc, char **argv) {
  return LunaDash::Templates::runModule<LunaDash::ControlClient>(argc, argv);
}
