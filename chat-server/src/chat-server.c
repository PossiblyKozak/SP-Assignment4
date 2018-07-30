/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>

#define _GNU_SOURCE
#define PORT_NUMBER 9030

char *activeUsernames[10];
pthread_t clientThreads[10];
int activeSockets[10];

int activeSocketCount = 0;
bool hasAnyoneConnected = false;

void error(const char *msg);
void *getMessages(void *newSocketID);
void *checkActiveClients();
void *getNewClients(void *tmpSocketID);
void sendToAllOtherSockets(int socketID, char* msg);
void removeClientAtIndex(int i);

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void *getMessages(void *newSocketID)
{
	char messageBuffer[79];
	int socket = *(int*)newSocketID;
	int n;
	while(1)
	{
	    memset(messageBuffer, 0, 79);
	    n = read(socket,messageBuffer,79);

	    if (messageBuffer[0] != 0 && messageBuffer[25] == '>')
	    {
		    printf("%s\n",messageBuffer);
	    	messageBuffer[25] = '<';
	    	messageBuffer[26] = '<';	    		
	    	n = 0;
	    	sendToAllOtherSockets(socket, messageBuffer);
		}
		else if (messageBuffer[0] != 0)
		{
			if (strstr(messageBuffer, ">>bye<<") != NULL)
	    	{
	    		for (int i = 0; i < activeSocketCount; i++)
	    		{
	    			if (activeSockets[i] == socket)
	    			{
	    				sprintf(messageBuffer, "[SERVER] >> User %s left the chatroom", activeUsernames[i]);
	    				printf("%s >> USER_QUIT\n", messageBuffer);
	    				sendToAllOtherSockets(socket, messageBuffer);
	    				removeClientAtIndex(i);
			    		pthread_exit(NULL);
	    			}
	    		}
	    	}
		}
	}
}

void removeClientAtIndex(int i)
{
	memmove(&activeSockets[i], &activeSockets[i+1], sizeof(activeSockets) - sizeof(activeSockets[i]));
	memmove(&activeUsernames[i], &activeUsernames[i+1], sizeof(activeUsernames) - sizeof(activeUsernames[i]));
	memmove(&clientThreads[i], &clientThreads[i+1], sizeof(clientThreads) - sizeof(clientThreads[i]));			    		
	activeSocketCount--;
}

void *checkActiveClients()
{
	while (activeSocketCount > 0 || !hasAnyoneConnected)
	{
		for (int i = 0; i < activeSocketCount; i++)
		{
			int socket = activeSockets[i];
			char str[80];
		    int bufsize=80;
		    char *buffer=malloc(bufsize);
	        memset(buffer,0,80);
	        if (!(recv(socket,buffer,bufsize, MSG_PEEK) < 0) && buffer != NULL)
	        {
	            int n = send(socket, buffer, sizeof(buffer), MSG_NOSIGNAL);
	            if (n == -1)
	            {
					sprintf(buffer, "[SERVER] >> User %s left the chatroom", activeUsernames[i]);
					printf("%s >> NON RESPONSIVE\n", buffer);
	    			sendToAllOtherSockets(socket, buffer);
	    			removeClientAtIndex(i);
	    			break;
	            }           
	        }
	        free(buffer);
		}
		sleep(1);		
	}
}

void *getNewClients(void *tmpSocketID)
{
	int socketID = *(int*)tmpSocketID;
	struct sockaddr_in client_address;
	socklen_t clilentSize;
	int newSocketID;

	while (activeSocketCount > 0 || !hasAnyoneConnected )
    {
    	while(activeSocketCount == 9){ sleep(1); }
		listen(socketID,5);
	    clilentSize = sizeof(client_address);
	    newSocketID = accept(socketID, (struct sockaddr *)&client_address, &clilentSize);
	    fcntl(newSocketID, F_SETFL, O_NONBLOCK);
		
	    if (newSocketID >= 0)
	    {
	    	hasAnyoneConnected = true;
	    	char message[80];
	    	char* userName = malloc(20);
	    	memset(message, 0, 80);	   		
			while(1)
			{
			    memset(userName, 0, 20);
			    if (read(newSocketID,userName,sizeof(userName)) >= 0 && userName != NULL)
			    {
			    	memset(message, 0, 80);				    	
			    	sprintf(message, "[SERVER] >> User %s entered the chatroom.", userName);
			    	printf("%s\n", message);
			    	sendToAllOtherSockets(socketID, message);
			        break;  
			    }
			}	    	
	    	pthread_t threads;
		    pthread_attr_t attr;
		    pthread_attr_init(&attr);
		    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		    clientThreads[activeSocketCount] = threads;
		    activeSockets[activeSocketCount] = newSocketID;
		    printf("%s\n", userName);
		    activeUsernames[activeSocketCount] = userName;
		    //memcpy(activeUsernames[activeSocketCount], userName, sizeof(userName));
		    printf("Copy Successful\n");
			activeSocketCount++;
			for (int i = 0; i < activeSocketCount; i++)
		    {
		    	printf("Socket: %d, userName: %s\n", activeSockets[i], activeUsernames[i]);
		    }
			//printf("Starting new thread with ID: %d\nThere are %d active sockets\n", newSocketID, activeSocketCount);
			pthread_create(&threads, &attr, getMessages, (void *)&newSocketID);
	    }
	}
}

void sendToAllOtherSockets(int socketID, char* msg)
{
	sprintf(msg, "%-80s", msg);
	for (int i = 0; i < activeSocketCount; i++)
	{
		if (activeSockets[i] != socketID)
		{
			write(activeSockets[i], msg, 80);
		}
	}
}

int main(int argc, char *argv[])
{
	memset(activeSockets, 0, sizeof(activeSockets));
	memset(activeUsernames, 0, sizeof(activeUsernames));
    int socketID;
    struct sockaddr_in server_address;

    socketID = socket(AF_INET, SOCK_STREAM, 0);

    if (socketID < 0)
    {
    	error("There was an error opening the socket");
    }

    memset((char *) &server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT_NUMBER);

    if (bind(socketID, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
    {
    	close(socketID);
    	if (bind(socketID, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
        	error("ERROR on binding");
    }

	pthread_t threads;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);	
	pthread_create(&threads, &attr, getNewClients, (void *)&socketID);

	pthread_t activityCheckThread;
	pthread_create(&activityCheckThread, &attr, checkActiveClients, NULL);

	while (activeSocketCount > 0 || !hasAnyoneConnected){ sleep(1); };
	printf("All clients are now offline.\nEnding program...\n");
	pthread_cancel(threads);
    
    close(socketID);
    return 0;
}
