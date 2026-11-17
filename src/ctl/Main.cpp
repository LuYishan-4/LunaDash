#include "ctl/command/ControlClient.hpp"

int main(int argc, char **argv) {
  LunaDash::ControlClient client;
  return client.run(argc, argv);
}
