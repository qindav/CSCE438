#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>

//inet_addr things?
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

//For globals
#include <sys/mman.h>

//Catch errors
#include <errno.h>
#include <signal.h>

#include <pthread.h>

//Simple things
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "interface.h"

#define BUFFER_LENGTH    250
#define FALSE              

using namespace std;

//netstat -lntp
//netstat -tulpn
void* beginChatRoom(void *arg);

//Chatroom structure
class chatRoom{
	public:
	int pid;		//Process ID
	int users;		//# Users
	int port;		//Port
	string name;	//Chatroom name
	int mysocket;	//Socket hosted on
	vector<int>persons;	//Stores client sockets
	
	//constructor
	chatRoom(int id, string n, int p) {
		pid = id;		
		name = n;		
		users = 0;		
		port = p;
	}
	
	//Add a user onto the list
	void AddOnePerson(int socketid){
		users+=1;
		mysocket = socketid;
		persons.push_back(mysocket);
	}
};
struct WorkThreadPara{
	int socketid;
};


chatRoom *room;
vector<chatRoom*> rooms;	//Stores a list of all active chat rooms
int roomPort = 8085;		//Starting port for connections

//Used with Chat Room threading
void* chat_work_thread(void* arg) {
	chatRoom *thisroom = (chatRoom*)arg;
	
	int newSocket = thisroom->mysocket;
	char buffer[256];
	
	//Handles all messages being sent to the chatroom, displays to all
	while(1){
    	int rc = recv(newSocket, buffer, sizeof(buffer), 0);
	    if(rc<=0){printf("recieve error\n"); break;}
	    /printf("%s Recieved: %s\n",thisroom->name.c_str(), buffer);

		for(int i=0; i<thisroom->persons.size(); i++){
			if(thisroom->persons[i] != newSocket){
				//SEND BACK
	    		rc = send(thisroom->persons[i], buffer, sizeof(buffer), 0);
	    		if(rc<0){printf("send back error\n");}
	    		printf("%s send: %s - %i to %i\n",thisroom->name.c_str(), buffer, rc, thisroom->persons[i]);	
			}
		}
		
	 
	}
}

//Used for creating chat rooms
void* beginChatRoom(void *arg) {
	//Used to keep track of the room's information
	chatRoom *thisroom = (chatRoom*)arg;

	int roomPort;
	string roomName;
	int roompid;

	roomPort = (*thisroom).port;
	roomName = (*thisroom).name;
	roompid = (*thisroom).pid;

	int sockfd, newSocket, ret;
	struct sockaddr_in serverAddr, newAddr;
	char returnMsg[256];
	socklen_t addr_size;

	char buffer[256];
	pid_t childpid;
	
	//Socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int option =1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	
	if(sockfd < 0){ printf("Error in connection.\n"); exit(1); }
	printf("Chatroom Server Socket is created.\n");

	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(roomPort);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	//Bind
	ret = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if(ret < 0){	printf("Error in binding port for chat room.\n"); exit(1); }
	printf("Chatroom Bind to port %d\n", roomPort);
	
	//Listen
	if(listen(sockfd, 10) == 0){ printf("Listening....\n"); }
	else{ strerror(errno); }
	
	
	while(1){
		//Accept
		newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
		if(newSocket < 0){ printf("Error in accepting.\n");  }
		printf("\nWelcome to the Chat Room\n");
		
		//Found a new person to handle
		thisroom->AddOnePerson(newSocket);
	
		//will handle the client for the chatroom
	    pthread_t chatworker;
	    if(pthread_create(&chatworker, NULL,chat_work_thread, thisroom)){ printf("\n thread create faild\n"); exit(1); }
	}
 
	close(sockfd);	
}

//handle CREATE command
string createRoom(string roomName) {
	//Make sure room doesn't already exist
	if(rooms.size() > 0)
		for(int i=0; i<rooms.size(); i++)
			if(rooms[i]->name == roomName)
				return "FAIL1";  //Already exists
     
     //Creates a room reference
     room = new chatRoom(1, roomName, roomPort);
	 rooms.push_back(room);
     roomPort++;
	 printf("Chatroom recorded to vector, size: %i ", rooms.size());
	
	 //Thread to make the new room active
	 pthread_t chatmainworker;
	 if(pthread_create(&chatmainworker, NULL, beginChatRoom, room)){
	   printf("\n thread create faild\n");
	   exit(1);
	 }	
 
	return "SUCC0";
}

//handle JOIN command
string joinRoom(string roomName){
	string result = "SUCC0 ";

	//make sure room exists
	if(rooms.size() == 0) return "FAIL2";
	
	//finds the room within the vector, returns port and #users
	for(int i=0; i<rooms.size(); i++) {
		if(rooms[i]->name == roomName) {
			cout<<"find room"<<endl;
			result = std::to_string(rooms[i]->port) + " " + std::to_string(rooms[i]->users);
			return "SUCC0 " + result;
		}
	}
	//Doesn't exist
	return "FAIL2";
}

//handle LIST command
string listRooms(){

    string result = "SUCC0";
    if(rooms.size() == 0)
        result = "SUCC0 (no rooms created)";
    //returns the list of rooms
    else{
        for(int i=0; i<rooms.size(); i++) {
		    result = result + " " + rooms[i]->name;
        }
    }
    //std::cout<<result<<endl;
    return result;
}

//handle DELETE commands
string deleteRoom(string roomName) {
	
	//make sure room exists
	if(rooms.size() == 0)
		return "FAIL2";
	
	for(int i=0; i<rooms.size(); i++) {
		if(rooms[i]->name == roomName) {	//in room now
			//Shoot warning
			for(int j=0; j<rooms[i]->persons.size();j++){
				int rc = send(rooms[i]->persons[j], "Closing Room", 12, 0);
				// if(rc<0){printf("Closing msg error\n"); exit(-1);}
				close(rooms[i]->persons[j]);
			}
			
			//Do a timeout here
			sleep(1);
			
			rooms.erase(rooms.begin() + i);	//erase this room from the chatRoom vector
			//kill(rooms[i]->pid, SIGKILL);	//kill child process handling this room
		
			return "SUCC0";
		}
	}
	//Doesn't exist
	return "FAIL2";
}



string processMessage(char* message)
{
	string returnMsg; //Return
	string cmd(message); // for processing
	
	cout<<cmd<<endl;
	
	//Decide command
	if(cmd.substr(0, 6) == "CREATE") // CREATES a room
		returnMsg = createRoom(cmd.substr(7, cmd.length()-7));
	else if (cmd.substr(0, 6) == "DELETE") // DELETES a room
		returnMsg = deleteRoom(cmd.substr(7, cmd.length()-7));
	else if (cmd.substr(0, 4) == "JOIN") // RETURNS host and port of a room
		returnMsg = joinRoom(cmd.substr(5, cmd.length()-5));
	else if (cmd.substr(0, 4) == "LIST")	//LIST existing rooms
		returnMsg = listRooms();
	else
		returnMsg = "INVAL";
	
	
	return returnMsg;
}

void* work_thread(void* arg) {
	WorkThreadPara *input = (WorkThreadPara*)arg;
	int newSocket = input->socketid;
	char buffer[256];
	
	//Handles every new connection and their message
	while(1){
		//RECIEVE
    	int rc = recv(newSocket, buffer, sizeof(buffer), 0);
	    if(rc<=0){ printf("chat room recieve error\n"); break; }
	    printf("Recieved: %s\n",buffer);
	      
	    //Process message
		string reply = processMessage(buffer);
	    const char* sendBack = reply.c_str();
	    printf("Processed: %s\n", sendBack);
	      
	    //SEND BACK
	    rc = send(newSocket, sendBack, reply.length()+1, 0);
	    if(rc<0){printf("send back error\n"); break;}
	     
	}
     
}

int main(int argc, char** argv) 
{
	//Get host and port number
	//./server 127.0.0.1 8080
	if (argc != 3) {
		fprintf(stderr, "usage: enter host address and port number\n");
		exit(1);
	}
	char* host = argv[1];
	int port = atoi(argv[2]);
	
	//Global variable things
	// roomPort = mmap(NULL, sizeof(roomPort), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    // *roomPort = 8081;

	//Now make the server 
	int sockfd, ret;
	struct sockaddr_in serverAddr;
	int newSocket;
	struct sockaddr_in newAddr;
	char returnMsg[256];
	socklen_t addr_size;

	char buffer[256];
	pid_t childpid;
	
	//SOCKET
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	int option =1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
	if(sockfd < 0){ printf("Error in connection.\n"); exit(1); }
	printf("Server Socket is created.\n");

	memset(&serverAddr, '\0', sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_addr.s_addr = inet_addr(host);
	
	//BIND
	ret = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
	if(ret < 0){	printf("Error in binding.\n"); exit(1); }
	printf("Bind to port %d\n", port);

	//LISTEN
	if(listen(sockfd, 10) == 0){ printf("Listening....\n"); }
	else{ printf("Error in binding to port.\n"); }

	while(1){
		//ACCEPT
		newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
		if(newSocket < 0){ printf("Error in accepting.\n");  }
		printf("\nConnection accepted from %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));
		
		// now handle each client
	    WorkThreadPara workerpara;
	    workerpara.socketid=newSocket;
	    
	    pthread_t worker;
	    if(pthread_create(&worker, NULL,work_thread, (void*)&workerpara )){
	    	printf("\n thread create faild\n");
	    	exit(1);
	    }
	}
	close(sockfd);
	return 0;
}