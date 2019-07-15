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
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

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
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

struct ServerInfo {
  std::string hostname;
  std::string port;
  bool isAvailable;
};
std::vector<ServerInfo> server_db;

//Vector that stores every client that has been created
std::vector<Client> client_db;

//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
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
        std::cout<<request->hostname()<<":"<<request->port()<<" is availabe again"<<std::endl;
    }
    else{
        ServerInfo server;
        server.hostname = request->hostname();
        server.port = request->port();
        server.isAvailable = true;
        server_db.push_back(server);
        std::cout<<request->hostname()<<":"<<request->port()<<" is added."<<std::endl;
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



  //master
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
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
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
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
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    if(role==router){
        //std::string hostname,port;
        //std::cout<<"DB size:"<<server_db.size()<<std::endl;
        for(int i = 0; i< server_db.size(); i++){
          if(server_db[i].isAvailable){
            reply->set_msg("Login Successful!");
            reply->set_hostname(server_db[i].hostname);
            reply->set_port(server_db[i].port);
            //std::cout<<server_db[i].hostname<<":"<<server_db[i].port<<" was routed"<<std::endl;
            break;
          }
        }
        
    }
    else{
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
    }
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
 
      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = time+" :: "+message.username()+":"+message.msg()+"\n";
      //"Set Stream" is the default message from the client to initialize the stream
      if(message.msg() != "Set Stream")
        user_file << fileinput;
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(c->stream==0)
      	  c->stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	    if(count < c->following_file_size-20){
              count++;
	      continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
	for(int i = 0; i<newest_twenty.size(); i++){
	  new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }    
        continue;
      }
      //Send the message to each follower's stream
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
  std::cout << "Server listening on " << server_address << std::endl;

  //std::cout << "DB master Server: " << server_db.size() << std::endl;
  server->Wait();
}
void RunMasterServer(std::string hostname, std::string port_no) {
  std::string login_info = "localhost:3010";
  std::unique_ptr<SNSService::Stub> stub_;
  stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    Regmessage request;
    request.set_hostname(hostname);
    request.set_port(port_no);
    Reply reply;
    ClientContext context;

    Status status = stub_->Register(&context, request, &reply);

  std::string server_address = hostname+":"+port_no;
  SNSServiceImpl service;
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

void RunSlave(std::string hostname, std::string port_no){
  std::string mode;
  if(role==slave)
    mode = "M";
  else
    mode = "R";
  
    pid_t c_pid, pid;
    int status;
    pid = getpid();
    //std::cout << "pid in main: " <<pid << std::endl;
    int s;
    while(1){
      //std::cout<<"process run again"<<std::endl;  
      c_pid = fork();
   
      if( c_pid == 0 ){  
	      pid = getpid();
        //int rc = setsid();
  
	      char *const args[8] = {"tsd", "-p", (char*)port_no.c_str(),"-h",(char*)hostname.c_str(),"-r",(char*)mode.c_str(), NULL};
	      //args[0]="tsd";
	      execv("./tsd", args);
     }
     //std::cout<<"Monitor:"<<c_pid<<std::endl;
     while (1)
     {
       int status;
       s = waitpid(c_pid, &status, WNOHANG);
       //cout<<"Monitor res" << s << "ID" << c_pid <<endl;
       if(s!=0){
         //Report to routing im not available
         //cout<<"Monitor break"<<c_pid<<endl;
          break;
       }
    }
  }
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  std::string hostname = "localhost";
  std::string mode="";
  int opt = 0;
    
  while ((opt = getopt(argc, argv, "p:h:r:")) != -1){
     
    switch(opt) {
      case 'p':
          port = optarg;break;
      case 'h':
          hostname = optarg;break;
      case 'r':
          mode = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
   
  if(mode=="R"){
    role=router;
    RunRoutingServer(hostname,port);
  }
  if(mode=="M"){
    role=master;
    RunMasterServer(hostname,port);
  }
  if(mode=="S"){
    role=slave;
    RunSlave(hostname,port);
  }
  if(mode=="Z"){
    role=router_slave;
    RunSlave(hostname,port);
  }
 

  return 0;
}
