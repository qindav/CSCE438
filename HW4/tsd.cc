/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h> 
#include <sys/wait.h>

#include "sns.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::Regmessage;

enum serverRole{
  router,
  master,
  slave,
  router_slave
} role;

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  std::vector<std::string> followers;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

struct ServerInfo {
  std::string hostname;
  std::string port;
  bool isAvailable;
  int clientCount;
};
std::vector<ServerInfo> server_db;

//Vector that stores every client that has been created
std::vector<Client> client_db;

 std::unique_ptr<SNSService::Stub> stubR_;  //to routing server
  std::string mode;
google::protobuf::Timestamp lastPostTime;
//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username){
      return index;
      
    }
    index++;
  }

  

  return -1;
}

int find_server(std::string hostname, std::string port){
  int index = 0;
  for(ServerInfo s : server_db){
    if(s.hostname == hostname && s.port == port)
      return index;
    index++;
  }
  return -1;
}

int find_server_available(){
  int index = 0;
  for(ServerInfo s : server_db){
    if(s.isAvailable==true)
      return index;
    index++;
  }
  return -1;
}
 

class SNSServiceImpl final : public SNSService::Service {
  //register
  Status Register(ServerContext* context, const Regmessage* request, Reply* reply) override {

    int index = find_server(request->hostname(),request->port());
    if(index>=0){
        ServerInfo server = server_db[index];
        server.isAvailable = true;
        std::cout<<request->hostname()<<":"<<request->port()<<" is available again"<<std::endl;
	server.clientCount=0;
    }
    else{
        ServerInfo server;
        server.hostname = request->hostname();
        server.port = request->port();
        server.isAvailable = true;
        server_db.push_back(server);
        std::cout<<request->hostname()<<":"<<request->port()<<" is added."<<std::endl << server.isAvailable<< std::endl;
        server.clientCount=0;
    }
    //index = find_server_available();
    //if(index<0){
    //   server.isAvailable = true;
    //}
    return Status::OK;
  }
  //report
  Status Report(ServerContext* context, const Regmessage* request, Reply* reply) override {

    int index = find_server(request->hostname(),request->port());
    if(index>=0){
        //ServerInfo server = server_db[index];
        //server.isAvailable = false;
        server_db[index].isAvailable = false;
        //turn on next one to available
        //for(int i = 0; i< server_db.size(); i++){
        //  if(i!=index)
        //  {
        //    server_db[i].isAvailable = true;
        //    break;
        //  }
        //}
        std::cout<<request->hostname()<<":"<<request->port()<<" is unavailabe"<<std::endl;
    }
  
    return Status::OK;
}

//for router server to publish user information
  Status Subscription(ServerContext* context, const Request* request, ListReply* list_reply1) override {
      Request request1;
      request1.set_username(request->username());

      //Container for the data from the server
       ListReply list_reply;

      //Context for the client
      ClientContext context1;

      Status status = stubR_->List(&context1, request1, &list_reply);
   
     
    
      Client c;
      std::string username = request->username();
      int user_index = find_user(username);
      if(user_index < 0){
        c.username = username;
        for(std::string s : list_reply.followers()){
            c.followers.push_back(s);
        }
        client_db.push_back(c);
      }
      else{ 
        Client *user = &client_db[user_index];
        user->followers.clear();
        for(std::string s : list_reply.followers()){
            user->followers.push_back(s);
        }
      }
	   
    
    return Status::OK;
  }

  //master
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
     if(server_db.size()>0){
	    //std::cout << "List function 0" << std::endl;

	    Client user = client_db[find_user(request->username())];
	    int index = 0;
	    for(Client c : client_db){
	      list_reply->add_all_users(c.username);
	    }

	    //std::cout << "List function 1" << std::endl;

	    std::vector<Client*>::const_iterator it;
	    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
	      list_reply->add_followers((*it)->username);
	    }

	    //std::cout << "List function 2" << std::endl;
    }
    else{
        ClientContext context1;
	Status status = stubR_->List(&context1, *request, list_reply);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    if(server_db.size()>0){
	    std::string username1 = request->username();
	    std::string username2 = request->arguments(0);
	    int join_index = find_user(username2);
	    if(join_index < 0 || username1 == username2)
	      reply->set_msg("Join Failed -- Invalid Username");
	    else{
	      Client *user1 = &client_db[find_user(username1)];
	      Client *user2 = &client_db[join_index];
	      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
		reply->set_msg("Join Failed -- Already Following User");
		return Status::OK;
	      }
	      user1->client_following.push_back(user2);
	      user2->client_followers.push_back(user1);
	      reply->set_msg("Join Successful");
	    }
    }
    else{
        std::cout<<"follow:"<<request->arguments(0)<<std::endl;
	ClientContext context1;
        Status status = stubR_->Follow(&context1, *request, reply);
        std::cout<<"followmsg:"<<reply->msg()<<std::endl;
    }
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    if(server_db.size()>0){
	    std::string username1 = request->username();
	    std::string username2 = request->arguments(0);
	    int leave_index = find_user(username2);
	    if(leave_index < 0 || username1 == username2)
	      reply->set_msg("Leave Failed -- Invalid Username");
	    else{
	      Client *user1 = &client_db[find_user(username1)];
	      Client *user2 = &client_db[leave_index];
	      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
		reply->set_msg("Leave Failed -- Not Following User");
		return Status::OK;
	      }
	      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
	      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
	      reply->set_msg("Leave Successful");
	    }
    }
   else{
	ClientContext context1;
        std::cout<<"unfollow:"<<request->arguments(0)<<std::endl;
        Status status = stubR_->UnFollow(&context1, *request, reply);
        std::cout<<"unfollowmsg:"<<reply->msg()<<std::endl;
 
   }

    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {

      //std::string hostname,port;
      std::cout<<"DB size:"<<server_db.size()<<std::endl<<"role:"<<role<<std::endl;
	
      if(server_db.size()>0){
	//router
	  int minc=1000;
          int minIndex=0;

          //find minimum load server
	  for(int i = 0; i< server_db.size(); i++){
	      if(server_db[i].isAvailable  && server_db[i].clientCount<=minc){
		   minc = server_db[i].clientCount;
		   minIndex = i;
	      }
          }
        

          //Find next available as slave server
	  int slaveIndex = -1;
	  for(int i = minIndex + 1; i< server_db.size(); i++){
	      if(server_db[i].isAvailable){
		 slaveIndex = i;  
	      }
          }
	  if(slaveIndex == -1){
              for(int i = 0; i< minIndex; i++){
	           if(server_db[i].isAvailable){
		        slaveIndex = i;
		   }  
	      }
          
	  }
	std::cout<<"server assigned:"<<minIndex<<", Slave assigned:"<<slaveIndex<<std::endl;	
	int i = minIndex;
 	reply->set_msg("Login Successful!");
	reply->set_hostname(server_db[i].hostname);
	reply->set_port(server_db[i].port);
	server_db[i].clientCount ++;
        std::cout<<server_db[i].hostname<<":"<<server_db[i].port<<" was routed"<<std::endl;
		  

	 if(slaveIndex == -1){
	    //Set slave server info
	    reply->set_slavehostname("");
	    reply->set_slaveport(""); 
	 }
         else{
            reply->set_slavehostname(server_db[slaveIndex].hostname);
	    reply->set_slaveport(server_db[slaveIndex].port);
         }
      }
    
/*
      for(int i = 0; i< server_db.size(); i++){
        if(server_db[i].isAvailable){
          reply->set_msg("Login Successful!");
          reply->set_hostname(server_db[i].hostname);
          reply->set_port(server_db[i].port);
	  server_db[i].clientCount ++;
          //std::cout<<server_db[i].hostname<<":"<<server_db[i].port<<" was routed"<<std::endl;
          break;
        }
      } 
*/    
      Client c;
      std::string username = request->username();
      int user_index = find_user(username);
      if(user_index < 0){
        c.username = username;
        client_db.push_back(c);
        reply->set_msg("Login Successful!");
      }
      else{ 
        Client *user = &client_db[user_index];
        if(user->connected)
          reply->set_msg("Invalid Username");
        else{
          std::string msg = "Welcome Back " + user->username;
	        reply->set_msg(msg);
          user->connected = true;
        }
      }
    
    return Status::OK;
  }

  Status Gettime(ServerContext* context,  const Request* request, Message* m) override {

	google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
        timestamp->set_seconds(time(NULL));
        timestamp->set_nanos(0);
        m->set_allocated_timestamp(timestamp);
       return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    Client *c;
    while(stream->Read(&message)) {
      std::string username = message.username();
      int user_index = find_user(username);
      c = &client_db[user_index];
 
      //get router time
      Message m;
      Request r;
      ClientContext context1;
      stubR_->Gettime(&context1, r, &m);
      google::protobuf::Timestamp Routertime = m.timestamp();
      //std::cout<<"Router time:"<<google::protobuf::util::TimeUtil::ToString(Routertime)<<std::endl;
      std::string globaltime =  google::protobuf::util::TimeUtil::ToString(Routertime);

      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = globaltime + " :: " + time+" :: "+message.username()+":"+message.msg()+"\n";
      //"Set Stream" is the default message from the client to initialize the stream

     //read last 20 message or latest message
      std::string line;
      std::vector<std::string> newest_twenty;
      std::ifstream in(username+"following.txt");
      int totalcount = 0;
      //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
      //check counts
      while(getline(in, line)){
	if(line.length()>20) 
           totalcount++;
      }
      //std::cout<<"Totalmsg:"<<totalcount<<std::endl;
      in.clear();
      in.seekg(0);
      int count = 0;
      while(getline(in, line)){
          if(line.length()>20){
	      count++;
              if(count>totalcount-20){
                  newest_twenty.push_back(line);
	      
              }
	  }
      }

      if(message.msg() != "Set Stream"){
        user_file << fileinput;
        //print latest message
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
        std::string msg;
        std::string hastimemsg;
        google::protobuf::Timestamp temptime;
	for(int i = 0; i<newest_twenty.size(); i++){
          msg = newest_twenty[i];
           google::protobuf::util::TimeUtil::FromString(msg.substr(0, 20), &temptime);
          if(temptime>lastPostTime){
	      new_msg.set_msg(msg);
              stream->Write(new_msg);
          }
         if(msg.length()>20) hastimemsg=msg;
        }  
        google::protobuf::util::TimeUtil::FromString(hastimemsg.substr(0, 20), &lastPostTime); 
        std::cout<< "last post time1 :"<< google::protobuf::util::TimeUtil::ToString(lastPostTime)  << std::endl;
     }
      else{
        //If message = "Set Stream", print the first 20 chats from the people you follow
      
        Message new_msg; 
        std::string msg;
        std::string hastimemsg;
 	//Send the newest messages to the client to be displayed
	for(int i = 0; i<newest_twenty.size(); i++){
          msg = newest_twenty[i];
	  new_msg.set_msg(msg);
          stream->Write(new_msg);
          if(msg.length()>20) hastimemsg=msg;
        }  
        google::protobuf::util::TimeUtil::FromString(hastimemsg.substr(0, 20), &lastPostTime); 
        std::cout<< "last post time:"<< google::protobuf::util::TimeUtil::ToString(lastPostTime)  << std::endl;
        continue;
      }
      //Send the message to each follower's stream
      /*
      std::vector<Client*>::const_iterator it;
      for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
        Client *temp_client = *it;
      	if(temp_client->stream!=0 && temp_client->connected)
	 temp_client->stream->Write(message);
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = temp_client->username;
        std::string temp_file = temp_username + "following.txt";
	std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	following_file << fileinput;
        temp_client->following_file_size++;
	std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }
      */
       
      for(int i=0; i<c->followers.size(); i++){

        std::string temp_username = c->followers[i];
        std::string temp_file = temp_username + "following.txt";
	std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	following_file << fileinput;
	std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }

    }
    //If the client disconnected from Chat Mode, set connected to false
    c->connected = false;
    return Status::OK;
  }

};

void RunRoutingServer(std::string hostname, std::string port_no) {
  std::string server_address = hostname+":"+port_no;
  SNSServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Routing Server listening on " << server_address << std::endl;


  //std::cout << "DB master Server: " << server_db.size() << std::endl;
  server->Wait();
}

void RunServer(std::string hostname, std::string port_no, std::string routingIP, std::string routingPort) {
  
  std::cout<<"Mode:"<< mode<<std::endl;
  //if(mode=="R"){
     //routingIP = "localhost:";
     //routingPort = "3010";
  // }
  std::string login_info = routingIP + ":" + routingPort;
  //std::unique_ptr<SNSService::Stub> stub_;
  stubR_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    Regmessage request;
    request.set_hostname(hostname);
    request.set_port(port_no);
    Reply reply;
    ClientContext context;

    Status status = stubR_->Register(&context, request, &reply);

  std::string server_address = hostname+":"+port_no;
  SNSServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Master Server listening on " << server_address << std::endl;

  server->Wait();
}

void RunSlave(std::string port_no, pid_t master_pid, std::string hostname) {

  std::cout << "Listening on slave server" << std::endl << "Master PID: " << master_pid << std::endl;
  pid_t slave_pid = getpid();
  std::cout << "Slave PID: " << slave_pid << std::endl;
  int status;
  while(1){

    status = waitpid(master_pid, NULL, 0);

    std::cout << status << std::endl;

    if(status == -1) {

      std::cout << "Master Server down, restarting..." << std::endl;
      break;

    }

    else {}

  }

  std::string argp = port_no;
  std::string argh = hostname;
  system("./startup.sh &");
  std::cout << "Successfully restarted master, shutting down..." << std::endl;
  exit(0);
  return;
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  std::string hostname = "localhost";
  std::string routingIP = "10.0.2.15";
  std::string routingPort = "16666";
  mode="";
  int opt = 0;
    
  while ((opt = getopt(argc, argv, "p:h:r:n:q:")) != -1){
     
    switch(opt) {
      case 'p':
          port = optarg;break;
      case 'h':
          hostname = optarg;break;
      case 'r':
          mode = optarg;break;
		   
	  case 'n':
	      routingIP = optarg;break;
	  case 'q':
          routingPort = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
   
pid_t pid;
  pid_t master_pid = getpid();

  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;

  int portno = atoi(port.c_str()); 
  

  server = gethostbyname(hostname.c_str());
 
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cout << "ERROR opening socket" << std::endl;
  }
 
  if (server == NULL) {
    fprintf(stderr,"ERROR, no such host\n");
    exit(0);
  }
 
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  int open = -1; 

  while((open == -1) && (portno < 16670)){

    serv_addr.sin_port = htons(portno);

    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {

      std::cout << "Port is closed" << std::endl;
      open = 0;

    } 

    else {

      std::cout << "Error: Port is active. Incrementing to next port" << std::endl;
      portno++;
      port = std::to_string(portno);
      std::cout << portno << std::endl << port << std::endl;
      open = -1;
       
    }


  }

  close(sockfd);

  if(portno > 16669) {

    std::cout << "Error, already four (4) servers up. Cannot create new server instance. Exiting..." << std::endl;
    exit(0);

  }

  pid = fork();

  if(pid != 0){ //not child, run slave to monitor child/master

      RunSlave(port, pid, hostname);

  }

  else {  //master server. We should check if routing server is up. If it is, set up on next free port
    std::cout<<"Port:"<<port<<std::endl;
    if(mode=="R" || (hostname=="10.0.2.15" && port=="16666")){
	    role = router;
        RunRoutingServer(hostname, port);
    }
    else{
		role = master;
		RunServer(hostname, port, routingIP, routingPort);
    }
    

  }


  return 0;
}
 

