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

pthread_t clientThreads[10];

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void *getMessages(void *newSocketID)
{
	char messageBuffer[256];
	int socket = *(int*)newSocketID;
	int n;
	while(1)
	{
		if (socket < 0) 
	    {
	        error("ERROR on accept");
	    }
	    memset(messageBuffer, 0, 256);
	    //bzero(messageBuffer,256);
	    n = read(socket,messageBuffer,255);

	    if (n < 0) 
	    {
	        error("ERROR reading from socket");
	    }

	    printf("%s - socketID: %d\n",messageBuffer, socket);
	    n = write(socket,"I got your message",18);

	    if (n < 0) 
	    {
	        error("ERROR writing to socket");
	    }
	}
}

int main(int argc, char *argv[])
{
    int portNumber, socketID, newSocketID;
    socklen_t clilentSize;
    struct sockaddr_in server_address, client_address;


    portNumber = 9037;
    socketID = socket(AF_INET, SOCK_STREAM, 0);

    if (socketID < 0)
    {
    	error("There was an error opening the socket");
    }

    bzero((char *) &server_address, sizeof(server_address));

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(portNumber);

    if (bind(socketID, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) 
    {
        error("ERROR on binding");
    }

    int i = 0;
    while (1)
    {
		listen(socketID,5);
	    clilentSize = sizeof(client_address);
	    newSocketID = accept(socketID, (struct sockaddr *)&client_address, &clilentSize);
    	pthread_t threads;
	    void *status;
	    pthread_attr_t attr;
	    pthread_attr_init(&attr);
	    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	    clientThreads[i] = threads;
		// Spawn the listen/receive deamons
		printf("Starting new thread with ID: %d\n", newSocketID);
		pthread_create(&threads, &attr, getMessages, (void *)&newSocketID);
		//pthread_create(&threads[1], &attr, listener, NULL);
	}
    
    close(newSocketID);
    close(socketID);
    return 0;
}
