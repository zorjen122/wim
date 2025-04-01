#include "state.grpc.pb.h"
#include "state.pb.h"

using state::ConnectUser;
using state::ConnectUserRsp;
using state::StateService;

class StateServiceImpl final : public StateService::Service {
public:
  grpc::Status GetImServer(grpc::ServerContext *context,
                           const ConnectUser *request,
                           ConnectUserRsp *response) override;
};