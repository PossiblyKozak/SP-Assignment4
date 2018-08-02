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

// Constants
#define _GNU_SOURCE
#define PORT_NUMBER 9030
#define NAME_MAX_CHARACTERS 15


// Globals
char activeUsernames[10][NAME_MAX_CHARACTERS];
int activeSockets[10];
pthread_t clientThreads[10];
int activeSocketCount = 0;
bool hasAnyoneConnected = false;

// Prototypes
void error(const char *msg);
void removeClientAtIndex(int i);
void sendToAllOtherSockets(int socketID, char* msg);
void *getMessages(void *newSocketID);
void *getNewClients(void *tmpSocketID);
void *checkActiveClients();

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void removeClientAtIndex(int i)
{
	memmove(&activeSockets[i], &activeSockets[i+1], sizeof(activeSockets) - sizeof(activeSockets[i]));
	memmove(&activeUsernames[i], &activeUsernames[i+1], sizeof(activeUsernames) - sizeof(activeUsernames[i]));
	memmove(&clientThreads[i], &clientThreads[i+1], sizeof(clientThreads) - sizeof(clientThreads[i]));			    		
	activeSocketCount--;
}

void sendToAllOtherSockets(int socketID, char* msg)
{
	//sprintf(msg, "%-80s", msg);
	for (int i = 0; i < activeSocketCount; i++)
	{
		if (activeSockets[i] != socketID)
		{
			write(activeSockets[i], msg, strlen(msg));
		}
	}
}

int getIndexBySocketID(int socketID)
{
	for (int i = 0; i < activeSocketCount; i++)
	{
		if (activeSockets[i] == socketID)
			return i;
	}
	return -1;
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

	    if (messageBuffer[0] != 0 && messageBuffer[25] == '>')
	    {
		    printf("%s\n",messageBuffer);
	    	messageBuffer[25] = '<';
	    	messageBuffer[26] = '<';	    		
	    	n = 0;
	    	sendToAllOtherSockets(socket, messageBuffer);
		}
		else if (messageBuffer[0] == '/')
		{
			if (strcmp(messageBuffer, "/users") == 0)
			{
				memset(messageBuffer, 0, 80);
				sprintf(messageBuffer, "%d Users: ", activeSocketCount);
				for (int i = 0; i < activeSocketCount; i++)
				{
					strcat(messageBuffer, activeUsernames[i]);
					if (activeSockets[i] == socket)
						strcat(messageBuffer, "(You)");
					if (i + 1 != activeSocketCount)
						strcat(messageBuffer, ", ");
				}
				write(socket, messageBuffer, sizeof(messageBuffer));
			}
			else if (strstr(messageBuffer, "/name ") != NULL)
			{
				char newName[NAME_MAX_CHARACTERS];
				memset(newName, 0, NAME_MAX_CHARACTERS);
				memmove(newName, &messageBuffer[6], strlen(messageBuffer) - 6);				

				if (strlen(newName) > 0)
				{
					char serverMessage[80];

					int socketIndex = getIndexBySocketID(socket);
					memset(serverMessage, 0, sizeof(serverMessage));
					sprintf(serverMessage, "[SERVER] >> User '%s' changed their name to '%s'.", activeUsernames[socketIndex], newName);
					memset(activeUsernames[socketIndex], 0, strlen(activeUsernames[socketIndex]));					

					strcpy(activeUsernames[socketIndex], newName);
					printf("%s\n", serverMessage);
					sendToAllOtherSockets(-1, serverMessage);

				}
			}			
		}
		else if (messageBuffer[0] != 0)
		{
			if (strcmp(messageBuffer, ">>bye<<") == 0)
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
	    	char* userName = malloc(40);
	    	memset(message, 0, 80);	   		
			while(1)
			{
			    if (read(newSocketID,activeUsernames[activeSocketCount],sizeof(activeUsernames[activeSocketCount])) >= 0 && activeUsernames[activeSocketCount] != NULL)
			    {
			    	memset(message, 0, 80);				    	
			    	sprintf(message, "[SERVER] >> User %s entered the chatroom.", activeUsernames[activeSocketCount]);
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
		    //strcpy(activeUsernames[activeSocketCount], userName);
			activeSocketCount++;

			for (int i = 0; i < activeSocketCount; i++)
		    {
		    	printf("Socket: %d, userName: %s\n", activeSockets[i], activeUsernames[i]);
		    }
			pthread_create(&threads, &attr, getMessages, (void *)&newSocketID);
	    }
	}
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
