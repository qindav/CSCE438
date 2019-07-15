#include <iostream>
//#include <memory>
//#include <thread>

#include <vector>
#include <string>
#include <sstream>

#include <unistd.h>

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>
#include <grpc++/security/credentials.h>

//#include "helper.h"

#include "timeline_client.h"
#include "timeline.grpc.pb.h"

#include <chrono>
#include <ctime>

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using namespace std;

class Client : public IClient
{
    public:
        Client(const std::string& hname,
            const std::string& uname,
            const std::string& p)
            :hostname(hname), myUsername(uname), port(p){}
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string myUsername;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<timeline::Stub> stub_;
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string myUsername = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                myUsername = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, myUsername, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
    std::string hosting = this->hostname + ":" + this->port;
    stub_ = timeline::NewStub(grpc::CreateChannel(hosting, grpc::InsecureChannelCredentials()));

    ClientContext context;
    username usr;
    Reply reply;
    IReply ire;
    reply.set_status(-99);
    usr.set_user(this->myUsername);
    grpc::Status status = stub_->acceptConnections(&context, usr, &reply);

     
    ire.comm_status = (IStatus) reply.status();

    if(reply.status() == -99)
        return -1;

    return 1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
    //Split tokens into the vector
    std::string buf;
    std::stringstream ss(input);
    std::vector<std::string> tokens;
    while (ss >> buf){

        tokens.push_back(buf);

    }

    //Set up variables here
    ClientContext context;    //Client that's being sent replies
    grpc::Status status;      //Success or Fail	
    Reply reply;              //Some possible reply
    fufArgs usrs;             //Username1 and Username2
    IReply ire;               //Their custom reply
	
    if(strstr(tokens[0].c_str(),"UNFOLLOW")) {

        usrs.set_clientname(this->myUsername);
        usrs.set_argname(tokens[1]);
        status = stub_->unfollow(&context, usrs, &reply);

        ire.comm_status = (IStatus) reply.status();

    }
    else if(strstr(tokens[0].c_str(),"FOLLOW")) {

        usrs.set_clientname(this->myUsername);
        usrs.set_argname(tokens[1]);
        status = stub_->follow(&context, usrs, &reply);
        ire.comm_status = (IStatus) reply.status();


    }
    else if(strstr(tokens[0].c_str(),"LIST")) {

        ListUsers list; username usr;
        usr.set_user(myUsername);
        std::unique_ptr<ClientReader<ListUsers>> reader(stub_->list(&context, usr));
        ire.comm_status = (IStatus) reply.status();

        //read from protocal buffer into ListUsers object list. Can now mess with list
        ire.following_users.push_back(myUsername);

        while(reader->Read(&list)) {    
 
            if(list.followed()==1)

                ire.following_users.push_back(list.user());

            else

                ire.all_users.push_back(list.user());

        }

    }

    else if(strstr(tokens[0].c_str(),"TIMELINE")) {

        std::cout << "Now you are in the timeline" << std::endl;
        processTimeline();

    }

    ire.grpc_status = status;
    return ire;
}

void Client::processTimeline()
{
    grpc::Status status;      //Success or Fail	
    Reply reply;              //Some possible reply
    ::posts po;               //Create message
    Empty empty;

 

    while(1){
        
        posts p; username usr;
        usr.set_user(this->myUsername);
        ClientContext context2;
        std::unique_ptr<ClientReader<posts>> reader(stub_->updateTimeline(&context2, usr));


        //read from protocal buffer into ListUsers object list 

        while(reader->Read(&p)) {

            //std::cout<<p.user()<<std::endl;
            //std::cout<<p.time()<<std::endl;
            //std::cout<<p.post()<<std::endl;
	    std::time_t t = (int)p.time();
           displayPostMessage(p.user(), p.post(), t);
        }       

        //Get current time and user message
        std::string message = getPostMessage();
        long int currTime = (long int)std::time(NULL);
        po.set_user(this->myUsername);
        po.set_time(currTime);
        po.set_post(message);

        

        //Send posts over
        ClientContext context;
        stub_->sendPost(&context,po,&empty);

         


    }

}
