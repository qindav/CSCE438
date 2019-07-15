#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>
//#include "helper.h"
#include "timeline.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
 


class clientEntry {
    public:
    std::string clientName;                     //clients name
    std::vector<std::string> followedList;      //who theyre following
    std::vector<std::string> followerList;      //who theyre following
    std::vector<posts> client_posts;            //list of their posts
    std::vector<posts> timeline;                //timeline
    int lastreadtime;
};

void runServer(const std::string host, const std::string port);
