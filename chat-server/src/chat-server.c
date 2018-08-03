/*	FILE			: chat-client.c
*	PROJECT			: PROG1970 - Assignment #4
*	PROGRAMMER		: Alex Kozak and Attila Katona
*	FIRST VERSION	: 2018-08-05
*	DESCRIPTION		: The chat client's purpose is to be the interface the user goes through in order to access the chatroom
*					  created and maintained by the chat-server application. The user has the ability to send and recieve messages
*					  from all other chat-clients connected to the same server. The client will sense if the connection with the
*					  server has been broken in any way and therefore disallow the sending of furthur messages and exiting itself.
*					  Other functions include listing active users in the chatroom, clearing the screen from old messages, parceling
*					  messages to a common size, and changing your own username. 
*/

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
#define PORT_NUMBER 		9030
#define NAME_MAX_CHARACTERS 15

#define MESSAGE_ARROW_INDEX 25

#define MESSAGE_LENGTH		80
#define MAX_ACTIVE_CLIENTS	10
#define MAX_NAME_LENGTH		15


// Globals
char activeUsernames[MAX_ACTIVE_CLIENTS][NAME_MAX_CHARACTERS];
int activeSockets[MAX_ACTIVE_CLIENTS];
pthread_t clientThreads[MAX_ACTIVE_CLIENTS];
int activeSocketCount = 0;
bool hasAnyoneConnected = false;

// Prototypes
void error(const char *msg);
void removeClientAtIndex(int i);
void sendToAllOtherSockets(int socketID, char* msg);
void *getMessages(void *newSocketID);
void *getNewClients(void *tmpSocketID);
void *checkActiveClients();
pthread_t newThread(void* threadFunction, void* threadArguments);

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

void changeName(char* messageBuffer, int socket)
{
	char newName[NAME_MAX_CHARACTERS];
	memset(newName, 0, NAME_MAX_CHARACTERS);
	memmove(newName, &messageBuffer[6], strlen(messageBuffer) - 6);				

	if (strlen(newName) > 0)
	{
		char serverMessage[MESSAGE_LENGTH];

		int socketIndex = getIndexBySocketID(socket);
		memset(serverMessage, 0, sizeof(serverMessage));
		sprintf(serverMessage, "[SERVER] >> User '%s' changed their name to '%s'.", activeUsernames[socketIndex], newName);
		memset(activeUsernames[socketIndex], 0, strlen(activeUsernames[socketIndex]));					
		strcpy(activeUsernames[socketIndex], newName);
		sendToAllOtherSockets(-1, serverMessage);
	}	
}

void printUsers(int socket)
{
	char messageBuffer[MESSAGE_LENGTH];
	memset(messageBuffer, 0, MESSAGE_LENGTH);
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

void exitProgram(char* messageBuffer, int socket)
{
		int index = getIndexBySocketID(socket);
		sprintf(messageBuffer, "[SERVER] >> User %s left the chatroom", activeUsernames[index]);
		sendToAllOtherSockets(socket, messageBuffer);
		removeClientAtIndex(index);
		pthread_exit(NULL);
}

void *getMessages(void *newSocketID)
{
	char messageBuffer[MESSAGE_LENGTH];
	int socket = *(int*)newSocketID;
	int n;
	while(1)
	{
	    memset(messageBuffer, 0, MESSAGE_LENGTH);
	    n = read(socket,messageBuffer,MESSAGE_LENGTH);

	    if (messageBuffer[0] != 0 && messageBuffer[MESSAGE_ARROW_INDEX] == '>')
	    {
		    printf("%s\n",messageBuffer);
	    	messageBuffer[MESSAGE_ARROW_INDEX] = '<';
	    	messageBuffer[MESSAGE_ARROW_INDEX+1] = '<';	    		
	    	sendToAllOtherSockets(socket, messageBuffer);
		}
		else if (messageBuffer[0] == '/')
		{
			if (strcmp(messageBuffer, "/users") == 0)
			{
				printUsers(socket);
			}
			else if (strstr(messageBuffer, "/name ") != NULL)
			{
				changeName(messageBuffer, socket);
			}			
		}
		else if (messageBuffer[0] != 0)
		{
			if (strcmp(messageBuffer, ">>bye<<") == 0)
	    	{
	    		exitProgram(messageBuffer, socket);
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
    	while(activeSocketCount == MAX_ACTIVE_CLIENTS - 1){ sleep(1); }
		listen(socketID,5);
	    clilentSize = sizeof(client_address);
	    newSocketID = accept(socketID, (struct sockaddr *)&client_address, &clilentSize);
	    fcntl(newSocketID, F_SETFL, O_NONBLOCK);
		
	    if (newSocketID >= 0)
	    {
	    	hasAnyoneConnected = true;
			while(1)
			{
			    if (read(newSocketID,activeUsernames[activeSocketCount],sizeof(activeUsernames[activeSocketCount])) >= 0 && activeUsernames[activeSocketCount] != NULL)
			    {
			    	char message[80];
			    	memset(message, 0, MESSAGE_LENGTH);				    	
			    	sprintf(message, "[SERVER] >> User %s entered the chatroom.", activeUsernames[activeSocketCount]);
			    	sendToAllOtherSockets(socketID, message);
			        break;  
			    }
			}	    	
		    pthread_t threads = newThread(getMessages, (void*)&newSocketID);

			clientThreads[activeSocketCount] = threads;
		    activeSockets[activeSocketCount++] = newSocketID;
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
		    int bufsize = MESSAGE_LENGTH;
		    char *buffer = malloc(bufsize);
	        memset(buffer,0,bufsize);
	        if (!(recv(socket,buffer,bufsize, MSG_PEEK) < 0) && buffer != NULL)
	        {
	            if (send(socket, buffer, sizeof(buffer), MSG_NOSIGNAL) == -1)
	            {
					sprintf(buffer, "[SERVER] >> User %s left the chatroom", activeUsernames[i]);
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

pthread_t newThread(void* threadFunction, void* threadArguments)
{
	pthread_t threads;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);	
	pthread_create(&threads, &attr, threadFunction, threadArguments);
	return threads;
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

	pthread_t threads = newThread(getNewClients, (void*)&socketID);
	pthread_t activityCheckThread = newThread(checkActiveClients, NULL);

	while (activeSocketCount > 0 || !hasAnyoneConnected){ sleep(1); };
	printf("All clients are now offline.\nEnding program...\n");
	pthread_cancel(threads);
    
    close(socketID);
    return 0;
}
