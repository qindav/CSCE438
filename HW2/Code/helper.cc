/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <iostream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "timeline.grpc.pb.h"

#include "timeline_server.h"
 
#include <unistd.h>
 posts * post;

void outputdb(std::vector<clientEntry> & Clients);
void ReadLong(long* l, std::string line) {
    *l=0;
    *l = std::stol(line.substr(0));
  }
  
void GetDbFileContent( const std::string db_path, std::vector<clientEntry>& Clients) {
    Clients.clear();
    std::string line;
    std::ifstream infile(db_path);
    clientEntry* client;
    while(std::getline(infile,line)){

       if(line.substr(0,12)=="[ClientName]"){
            client = new clientEntry;
            client->clientName = line.substr(13);
            client->lastreadtime = 0; 
            Clients.push_back(*client);
       }
       else if(line.substr(0,10)=="[Followed]"){
            Clients.at(Clients.size()-1).followedList.push_back( line.substr(11));
       }
       else if(line.substr(0,10)=="[Follower]"){
	   Clients.at(Clients.size()-1).followerList.push_back( line.substr(11));
       }
       else if(line.substr(0,6)=="[Post]"){
            post = new posts;
	    std::getline(infile,line);
            post->set_user(line);
	    std::getline(infile,line);
            long temp=0;
            ReadLong(&temp, line);
            post->set_time(temp);
	    std::getline(infile,line);
            post->set_post(line);
            
	    Clients.at(Clients.size()-1).client_posts.push_back(*post);
       }
	else if(line.substr(0,10)=="[Timeline]"){
	    post = new posts();
	    std::getline(infile,line);
            post->set_user(line);
	    std::getline(infile,line);
            long temp=0;
            ReadLong(&temp, line);
            post->set_time(temp);
	    std::getline(infile,line);
            post->set_post(line);
	    Clients.at(Clients.size()-1).timeline.push_back(*post);
       }

    }
    
    
    infile.close();
    
}


void WriteToDB(const std::string& db_path, std::vector<clientEntry>& Clients){
    clientEntry* client;
    std::ofstream fo(db_path);
    for(int i=0; i<Clients.size(); i++){
        client = &Clients.at(i);
        fo<<"[ClientName] " + client->clientName << std::endl;
        for(int j=0; j<client->followedList.size(); j++){
            fo<<"[Followed] " + client->followedList.at(j) << std::endl;
        }
        for(int j=0; j<client->followerList.size(); j++){
            fo<<"[Follower] " + client->followerList.at(j) << std::endl;
        } 
        if(client->client_posts.size()>0){
            
            for(int j=0; j<client->client_posts.size(); j++){
	        fo<<"[Post]"<<std::endl;
                posts* post = &client->client_posts.at(j);
                fo<<post->user()<<std::endl;
		fo<<post->time()<<std::endl;
		fo<<post->post()<<std::endl;
            }
        }
        if(client->timeline.size()>0){
            
            for(int j=0; j<client->timeline.size(); j++){
	        fo<<"[Timeline]"<<std::endl;
                posts* post = &client->timeline.at(j);
                fo<<post->user()<<std::endl;
		fo<<post->time()<<std::endl;
		fo<<post->post()<<std::endl;
            }
        }
   }  
   fo.close();              
}

void outputdb(std::vector<clientEntry> & Clients){
    clientEntry* client;
    
    for(int i=0; i<Clients.size(); i++){
        client = &Clients.at(i);
        std::cout<<"[ClientName] " <<  client->clientName << std::endl;
        std::cout<<"[Followed] " << client->followedList.size() << std::endl;
	std::cout<<"[Follower] " << client->followerList.size() << std::endl;
	std::cout<<"[self post] " << client->client_posts.size() << std::endl;
	std::cout<<"[timeline] " << client->timeline.size() << std::endl;
	std::cout<<"[LastReadTime] " <<  client->lastreadtime << std::endl;
        for(int j=0; j<client->followedList.size(); j++){
            std::cout<<"[Followed] " << client->followedList.at(j) << std::endl;
        }
        for(int j=0; j<client->followerList.size(); j++){
            std::cout<<"[Follower] " << client->followerList.at(j) << std::endl;
        } 
        if(client->client_posts.size()>0){
            
            for(int j=0; j<client->client_posts.size(); j++){
	        std::cout<<"[Post]"<<std::endl;
                posts* post = &client->client_posts.at(j);
                std::cout<<post->user()<<std::endl;
		std::cout<<post->time()<<std::endl;
		std::cout<<post->post()<<std::endl;
            }
        }
        if(client->timeline.size()>0){
            
            for(int j=0; j<client->timeline.size(); j++){
	        std::cout<<"[timeline]"<<std::endl;
                posts* post = &client->timeline.at(j);
                std::cout<<post->user()<<std::endl;
		std::cout<<post->time()<<std::endl;
		std::cout<<post->post()<<std::endl;
            }
        }
   }  
              
}
 

