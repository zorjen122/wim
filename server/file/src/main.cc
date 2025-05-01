#include "Service.h"

int main() {
  wim::rpc::FileServer fileServer(2);

  fileServer.Run(7272);

  return 0;
}