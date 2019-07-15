#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

//inet_addr things?
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

#define BUFFER_LENGTH    250


int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);
void* process_recv(void* arg);


using namespace std;

int main(int argc, char** argv) 
{
	// Use: ./client 127.0.0.1 8080
    if (argc != 3) {
        fprintf(stderr, "usage: enter host address and port number\n");
        exit(1);
    }

    display_title();
    //create a connection
    int sockfd = connect_to(argv[1], atoi(argv[2]));
    while (1) {
        //reads from command line input
        char command[MAX_DATA];
        get_command(command, MAX_DATA);
        
        //send and recieve a reply from the server
        struct Reply reply = process_command(sockfd, command);
        display_reply(command, reply);
		
        touppercase(command, strlen(command) - 1);
		
        if (reply.status==SUCCESS && strncmp(command, "JOIN", 4) == 0) {
            printf("Now you are in the chatmode\n");
            process_chatmode(argv[1], reply.port);
        }
    //close(sockfd);
    }
    return 0;
}

/* Connect to the server using given host and port information */
int connect_to(const char *server, const int port)
{
    int sd, rc;
    struct sockaddr_in serveraddr;
    struct hostent *hostp;
    
    printf("port:%i  %s\n", port, server);
    //SOCKET
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if(sd<0){printf("socket error\n"); exit(-1);}
    
    //preps to get host name
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family      = AF_INET;
    serveraddr.sin_port        = htons(port);
    serveraddr.sin_addr.s_addr = inet_addr(server);
    
	  //GETHOSTNAME
    if (serveraddr.sin_addr.s_addr == (unsigned long)INADDR_NONE) {
        hostp = gethostbyname(server);
        if (hostp == (struct hostent *)NULL) { printf("host error\n"); exit(-1);}
        memcpy(&serveraddr.sin_addr, hostp->h_addr, sizeof(serveraddr.sin_addr));
    }
    
    //CONNECT
    rc = connect(sd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    if(rc<0){printf("connect_to error\n"); exit(-1);}
    
    //Done
    return sd;
}

/* Send an input command to the server and return the result */
struct Reply process_command(const int sd, char* command)
{
    //Create a buffer for the message
    char buffer[BUFFER_LENGTH];
    strcpy(buffer,command);
	
    //SEND
    int rc = send(sd, buffer, sizeof(buffer), 0);
    if(rc<0){printf("send error\n"); exit(-1);}
    
    //RECIEVE
    memset(buffer,'\0',sizeof(buffer));
  	rc = recv(sd, buffer,  200, 0);
		//printf("Client Recieved: %s\n", buffer);

    //Now create the reply
    struct Reply reply;
    string responseString(buffer); // for processing
	
    //Statuses:
    if(responseString.substr(0, 5) == "SUCC0"){
        //No extra parameters for CREATE/DELETE
        reply.status = SUCCESS;
    if(strncmp(command, "LIST", 4) == 0){
        //Looks like: SUCC0 rm1 rm2 ...
        string temp = responseString.erase(0, 6);
        reply.list_room = (char*)temp.c_str();
    }
    else if(strncmp(command, "JOIN", 4) == 0){
        //Looks like: SUCC0 3000 3
        //Assumes we only have <10 users and 4 digit ports
        reply.num_member = stoi(responseString.substr(11, 1));
        reply.port = stoi(responseString.substr(6, 4));
    }
    }

    //Shows failures
    else if(responseString.substr(0, 5) == "FAIL1")
        reply.status = FAILURE_ALREADY_EXISTS;
    else if(responseString.substr(0, 5) == "FAIL2")
        reply.status = FAILURE_NOT_EXISTS;
    else if(responseString.substr(0, 5) == "FAIL3")
        reply.status = FAILURE_INVALID;
    else if(responseString.substr(0, 5) == "FAIL4")
        reply.status = FAILURE_UNKNOWN;
    else{ // Invalid Command
        reply.status = FAILURE_INVALID;
        return reply;
    }
    return reply;
}

/*  Get into the chat mode */
void process_chatmode(const char* server, const int port)
{
    //Reuse conenct to
    int sd = connect_to(server, port);
	
    //Create a thread for the client to join the server
    pthread_t rworker;
    pthread_create(&rworker, NULL,process_recv, &sd );
	
    //Now loops on the server until time ends
    while(1){
        //gets message from user
        char sbuffer[BUFFER_LENGTH];
        char rbuffer[BUFFER_LENGTH];
        get_message(sbuffer,BUFFER_LENGTH);
		
        //SEND
        int rc = send(sd, sbuffer, sizeof(sbuffer), 0);
        if(rc<0){printf("chat send error\n"); exit(1);}
	}
}

void* process_recv(void* arg)
{
    //Handles recieves
    int sd;
    sd = *((int *)arg);
    char rbuffer[BUFFER_LENGTH];
    
    while(1) {
    //RECIEVE 
	  ssize_t rcn = recv(sd, rbuffer,  BUFFER_LENGTH, 0); 
	  if(rcn<=0){ printf("client receive error, chat room might closed\n"); break; }
    else{
        rbuffer[rcn]='\0';
        if(rbuffer[0]>0) {
            //printf("receive %s %i from %i %i\n", rbuffer,rcn, sd, rbuffer[0]);
            display_message(rbuffer);
	  		}
	   }
    }
  
}
