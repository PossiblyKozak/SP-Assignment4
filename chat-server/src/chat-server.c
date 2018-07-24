/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int portNumber, socketID, newSocketID, n;
    socklen_t clilentSize;
    char messageBuffer[256];
    struct sockaddr_in server_address, client_address;


    portNumber = 9035;
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

    while (1)
    {
	    listen(socketID,5);
	    clilentSize = sizeof(client_address);
	    newSocketID = accept(socketID, (struct sockaddr *)&client_address, &clilentSize);

	    if (newSocketID < 0) 
	    {
	        error("ERROR on accept");
	    }
	    bzero(messageBuffer,256);
	    n = read(newSocketID,messageBuffer,255);

	    if (n < 0) 
	    {
	        error("ERROR reading from socket");
	    }

	    printf("Here is the message: %s\n",messageBuffer);
	    n = write(newSocketID,"I got your message",18);

	    if (n < 0) 
	    {
	        error("ERROR writing to socket");
	    }
	}
    
    close(newSocketID);
    close(socketID);
    return 0;
}
