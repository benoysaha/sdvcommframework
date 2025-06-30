#include "my_sample_rpc_impl.h"
#include <iostream>
#include <google/protobuf/stubs/common.h> // For GOOGLE_PROTOBUF_VERIFY_VERSION

namespace comms_stack {

MySampleRpcImpl::MySampleRpcImpl() {
    GOOGLE_PROTOBUF_VERIFY_VERSION; // Good practice
    std::cout << "MySampleRpcImpl: Created." << std::endl;
}

MySampleRpcImpl::~MySampleRpcImpl() {
    std::cout << "MySampleRpcImpl: Destroyed." << std::endl;
}

void MySampleRpcImpl::Echo(::google::protobuf::RpcController* controller,
                           const protos::EchoRequest* request,
                           protos::EchoResponse* response,
                           ::google::protobuf::Closure* done) {
    std::cout << "MySampleRpcImpl::Echo called with message: " << request->request_message() << std::endl;
    response->set_response_message("Echo from server: " + request->request_message());
    if (done) {
        done->Run();
    }
}

void MySampleRpcImpl::Add(::google::protobuf::RpcController* controller,
                          const protos::AddRequest* request,
                          protos::AddResponse* response,
                          ::google::protobuf::Closure* done) {
    std::cout << "MySampleRpcImpl::Add called with a=" << request->a() << ", b=" << request->b() << std::endl;
    response->set_sum(request->a() + request->b());
    if (done) {
        done->Run();
    }
}

} // namespace comms_stack
