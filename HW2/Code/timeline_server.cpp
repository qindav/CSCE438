#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <fstream>
#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>
//#include "timeline.grpc.pb.h"
#include "timeline_server.h"
#include "helper.h"
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;
 
using std::chrono::system_clock;


class timelineImpl final : public timeline::Service {

    public:

        std::vector<clientEntry> clients;
        std::vector<clientEntry> not_connected;

        //Used for connectto, begins a timeline for all users
        ::grpc::Status acceptConnections(::grpc::ServerContext* context, const ::username* username, ::Reply* response) override {

            //great, we have a new user. So lets add it to clients. 
            //Also need check if user exits, because user can exit and run again

            int j = 0, client = -1;

            for(j = 0; j < clients.size(); j++){

                if(username->user() == clients.at(j).clientName)    //found index for client

                client = j;

            }
/*
            for(j = 0; j < not_connected.size(); j++) {

                if(username->user() == not_connected.at(j).clientName){

                    clientEntry * newClient = new clientEntry;
                    newClient->clientName = username->user();
                    newClient->followedList = not_connected.at(j).followedList;
                    newClient->followerList = not_connected.at(j).followerList;
                    newClient->client_posts = not_connected.at(j).client_posts;
                    newClient->timeline = not_connected.at(j).timeline;

                    response->set_status(0);
                    return Status::OK;

                }

            }
*/
            if(client == -1){

                clientEntry *newClient = new clientEntry;
                newClient->clientName = username->user();
	        newClient->lastreadtime = 0;
                clients.push_back(*newClient);

	            response->set_status(0);

		   return Status::OK;

            }

            else{
		clients.at(client).lastreadtime = 0;
                response->set_status(0);
                return Status::OK;

            }
        }

        //FOLLOW <username>: Follows the named user's timeline

        ::grpc::Status follow(::grpc::ServerContext* context, const ::fufArgs* args, ::Reply* response) override {
            std::string clientName = args->clientname();
            std::string followName = args->argname();


            //Someone is trying to follow themself??

            if(clientName == followName){

                response->set_status(1);
                return Status::OK;

            }
            
            //Find indexes

            int j = 0, client = -1, follow = -1;

            for(j = 0; j < clients.size(); j++) {

                if(clientName == clients.at(j).clientName) {   

                    client = j; //found index for client

                }

                if(followName == clients.at(j).clientName){

                    follow = j; //found index for follower
                }

            }

            //Do the follow here

            if(client == -1){ // couldn't find the client in database
                response->set_status(2);
                return Status::OK;
            }
            else if(follow == -1){ // couldn't find the follower in database
                response->set_status(2);
                return Status::OK;
            }

            else {//add follow username to client followed users vector

    		int  k;
                int  c = -1;

                for(k = 0; k < clients.at(client).followedList.size(); k++){

                    if(clients.at(client).followedList.at(k)==followName){

                        //already follow
                        c=k;
                        response->set_status(3);
                        return Status::OK;

                    }
                
                }

                if(c==-1){

                    //the name I followed
                    clients.at(client).followedList.push_back(followName);
                    //put me into follower of the other user
                    clients.at(follow).followerList.push_back(clientName);
                        
                }
                //Success!
                    response->set_status(0);
                    return Status::OK;
            }

        }

        //UNFOLLOW <username>: Unfollows the named user's timeline
        ::grpc::Status unfollow(::grpc::ServerContext* context, const ::fufArgs* args, ::Reply* response) override {
            //Prepping variables
            std::string clientName = args->clientname();
            std::string followName = args->argname();


            //Someone is trying to follow themself??
            if(clientName == followName){
                response->set_status(3);
                return Status::OK;
            }

            //Find indexes
            int j = 0, client = -1, unfollow = -1;
            for(j = 0; j < clients.size(); j++) {
                if(clientName == clients.at(j).clientName)    
                    client = j; //found index for client
                if(followName == clients.at(j).clientName)
                    unfollow = j; //found index for follower
            }

            //Do the unfollow here
            if(client == -1){ // couldn't find the client in database
                response->set_status(2);
                return Status::OK;
            }
            else if(unfollow == -1){ // couldn't find the follower in database
                response->set_status(2); 
                return Status::OK;
            }
            else {//delete from followed users vector
  		j=0;
                int c=-1;
       		for(j = 0; j <  clients.at(client).followedList.size(); j++) {
                    if(followName == clients.at(client).followedList.at(j)) {    //found in followed list
                        c = j;
                        break;
                    }
                }
        	if(c!=-1){
                     std::cout<<"off follower"<<c<<std::endl;
                     clients.at(client).followedList.erase(clients.at(client).followedList.begin() + c);
                }
		else{
                    response->set_status(3);
                    return Status::OK;
		}

           	std::cout<<"off follower"<<std::endl;
         	//take me off follower list
       		j = 0;
                c = -1;
	        for(j = 0; j <  clients.at(unfollow).followerList.size(); j++) {
                    if(clientName == clients.at(unfollow).followerList.at(j)) {    //found index for client
             		c = j;
                        break;
                    }
                }

        	if(c!=-1)
                    clients.at(unfollow).followerList.erase(clients.at(unfollow).followerList.begin() + c);
                
               //Success!
               response->set_status(0);
               return Status::OK;

           }

        }

         
        ::grpc::Status list(::grpc::ServerContext* context, const ::username* user, ::grpc::ServerWriter< ::ListUsers>* writer) override {
            std::cout<<" LIST";
            std::string connected_client = user->user();
            int client_place = -1;

            int j = 0;

            ListUsers * newUser = new ListUsers;
            //return whole list
            for(j = 0; j < clients.size(); j++){
       		//write whole user list
        	newUser->set_user(clients.at(j).clientName);
                newUser->set_followed(0);
       		writer->Write(*newUser);

       		 //Find client list
                if(connected_client == clients.at(j).clientName) {
                    client_place = j;
                }
            }
        
         
       	    //return follow list
            int k = 0;
            for(k = 0; k < clients.at(client_place).followedList.size(); k++){
                newUser->set_user(clients.at(client_place).followedList.at(k));
                newUser->set_followed(1);
                writer->Write(*newUser);
            }
        delete newUser;
        return Status::OK;
        }

        
        

        
        ::grpc::Status updateTimeline(::grpc::ServerContext* context, const ::username * user, ::grpc::ServerWriter<::posts>* writer) override {

            //std::cout << "HELLO" << std::endl;
            std::string connected_client = user->user();
            int client_place = -1, j = 0; 
            for(j = 0; j < clients.size(); j++){
                if(connected_client == clients.at(j).clientName) {
                    client_place = j; 
                }
            }
            if(client_place == -1) {
                return Status(StatusCode::FAILED_PRECONDITION, "We couldn't find you in our database");
            }
	    //std::cout<< "Here in update" << std::endl;
            int l = 0;
            for(l = 0; ((l < 20) && (l < clients.at(client_place).timeline.size())); l++) {
               
                posts post = clients.at(client_place).timeline.at(clients.at(client_place).timeline.size() - l - 1);
                if(post.time()>clients.at(client_place).lastreadtime){
                 //std::cout<< "Here updating" <<clients.at(client_place).lastreadtime<<":"<<post.time()<< std::endl;
                    writer->Write(post);
		}
            }
	    clients.at(client_place).lastreadtime = (long int)std::time(NULL);
            return Status::OK;
        }
        
        //::grpc::Status sendPost(::grpc::ServerContext* context, const ::posts* request, ::Empty* response)
        ::grpc::Status sendPost(::grpc::ServerContext* context, const ::posts* post, ::Empty* empty) override {
            std::string client_name = post->user();

            //Find client index
            int client_place = -1, j = 0; 
            for(j = 0; j < clients.size(); j++)
                if(client_name == clients.at(j).clientName) 
                    client_place = j;

            if(client_place == -1)
                return Status(StatusCode::FAILED_PRECONDITION, "We couldn't find you in our database");
           
            clients.at(client_place).client_posts.push_back(*post);
            //clients.at(client_place).timeline.push_back(*post);

            //put in my followers timeline
	    int k = -1;
            for(k = 0; k < clients.at(client_place).followerList.size(); k ++){
                int j=-1;
                for(j = 0; j < clients.size(); j++){
                    if(clients.at(j).clientName == clients.at(client_place).followerList.at(k)){
                        clients.at(j).timeline.push_back(*post);
                     }
                }
            }
        WriteToDB("timeline_db.txt", clients);
        return Status::OK;    
        }
};

void runServer(const std::string host, const std::string port) {

    std::string serverAddress = host + ":" + port;
    timelineImpl service;
    GetDbFileContent("timeline_db.txt", service.clients);
     //    std::cout<<"output:"<<service.clients.size()<<std::endl;
       // outputdb(service.clients);

// WriteToDB("timeline_db1.txt", service.clients); 
    ServerBuilder builder;
    builder.AddListeningPort(serverAddress, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << serverAddress << std::endl;
    server->Wait();
}

int main(int argc, char** argv) {
    std::string hostname="localhost";
    std::string port="3010";
    if(argc>1){
	port=argv[1];		
    }
    runServer(hostname, port);
    return 0;
}
