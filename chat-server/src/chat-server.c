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
#include <stdbool.h>

#define _GNU_SOURCE

char *activeUsernames[10];
pthread_t clientThreads[10];
int activeSockets[10];
int activeSocketCount = 0;
bool hasAnyoneConnected = false;

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void *getMessages(void *newSocketID)
{
	char messageBuffer[80];
	int socket = *(int*)newSocketID;
	int n;
	while(1)
	{
	    memset(messageBuffer, 0, 80);
	    n = read(socket,messageBuffer,79);
	    if (n < 0) 
	    {
	        error("ERROR reading from socket");
	    }

	    if (messageBuffer[0] != 0)
	    {
		    printf("%s\n",messageBuffer);
		    for (int i = 0; i < activeSocketCount; i++)
		    {
		    	if (strstr(messageBuffer, ">>bye<<") != NULL)
		    	{
		    		printf("User at socket %d left the chatroom\n", socket);
		    		activeSocketCount--;
		    		pthread_exit(NULL);
		    	}
		    	if (activeSockets[i] != socket)
		    	{
		    		messageBuffer[25] = '<';
		    		messageBuffer[26] = '<';
		    		n = write(activeSockets[i], messageBuffer, 80);
		    	}
			    if (n < 0) 
			    {
			        error("ERROR writing to socket");
			    }
		    }	    
		}
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
		listen(socketID,5);
	    clilentSize = sizeof(client_address);
	    newSocketID = accept(socketID, (struct sockaddr *)&client_address, &clilentSize);
		
	    if (newSocketID >= 0)
	    {
	    	printf("Someone connected!\n");
	    	hasAnyoneConnected = true;
	    	char userName[80], message[80];
	    	memset(message, 0, 80);	   		
			while(1)
			{
			    memset(userName, 0, 80);
			    //Receive message from server
			    read(newSocketID,userName,sizeof(userName));

			    //Print on own terminal
			    if (userName != NULL)
			    {
			        break;  
			    }
			}
	    	sprintf(message, "User %s entered the chatroom.", userName);

	    	for (int i = 0; i < activeSocketCount; i++)
		    {
		    	if (activeSockets[i] != socketID)
		    	{
		    		if (write(activeSockets[i], message, sizeof(message)) < 0)
		    		{
		  				error("ERROR writing to socket");
		    		}
		    	}
		    }

	    	pthread_t threads;
		    pthread_attr_t attr;
		    pthread_attr_init(&attr);
		    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		    clientThreads[activeSocketCount] = threads;
		    activeSockets[activeSocketCount] = newSocketID;

			printf("Starting new thread with ID: %d\n", newSocketID);
			pthread_create(&threads, &attr, getMessages, (void *)&newSocketID);
			activeSocketCount++;
	    }
	}
}

int main(int argc, char *argv[])
{
    int portNumber, socketID;
    struct sockaddr_in server_address;

///////////////////////
    portNumber = 9030;
///////////////////////

    socketID = socket(AF_INET, SOCK_STREAM, 0);

    if (socketID < 0)
    {
    	error("There was an error opening the socket");
    }

    memset((char *) &server_address, 0, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portNumber);

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

	while (activeSocketCount > 0 || !hasAnyoneConnected){sleep(1);};
	pthread_cancel(threads);
    
    close(socketID);
    return 0;
}
