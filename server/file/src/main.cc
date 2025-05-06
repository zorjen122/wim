#include "Service.h"

int main() {
  wim::rpc::FileServer fileServer(2);

  fileServer.Run(51000);
  return 0;
}