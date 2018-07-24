/*	FILE			: DR.c
*	PROJECT			: PROG1970 - Assignment #3
*	PROGRAMMER		: Alex Kozak and Attila Katona
*	FIRST VERSION	: 2018-07-14
*	DESCRIPTION		: The data reader (DR) program’s purpose is to monitor its incoming message queue for the varying
*					  statuses of the different Hoochamacallit machines on the shop floor. It will keep track of the number
*					  of different machines present in the system, and each new message it gets from a machine, it reports
*					  it findings to a data monitoring log file. The DR application is solely responsible for creating
*					  its incoming message queue and when it detects that all of its feeding machines have gone off-line, the
*					  DR application will free up the message queue, release its shared memory and the DR application will
*					  terminate.
*/
#include <pthread.h>
#include <curses.h>
#include <termios.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

// Prototypes
bool parseArgument(char *userID, char *serverName, char *argv);
void *sendmessage(void *name);
void *get_in_addr(struct sockaddr *sa);
void *listener();

int sockfd;
int done = 0;
WINDOW *top;
WINDOW *bottom;
int line=1; // Line position of top
int input=1; // Line position of top
int maxx,maxy; // Screen dimensions
pthread_mutex_t mutexsum = PTHREAD_MUTEX_INITIALIZER;  


// current IP: 10.113.16.20
int main(int argc, char *argv[])
{
	char userID[30], serverName[30];

	if ((argc == 3 && (!parseArgument(userID, serverName, argv[1]) || !parseArgument(userID, serverName, argv[2]))) || argc != 3)
	{
		printf("Proper Usage: chat-client -user<userID> -server<server name>\n");
	}
	else
	{
		printf("UserID: %s\n", userID);
		printf("Server Name: %s\n", serverName);
	}


	int len;
    int result;
    char buf[256];
    struct sockaddr_in address;

    // Make socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    // attr
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1");
    address.sin_port = htons(9035);

    len = sizeof(address);

    // Make connection
    result = connect(sockfd, (struct sockaddr *)&address, len);

    printf("\n\nBuilding connection\n\n");

    if(result == -1)
    {
        perror("\nConnection failed, try again.\n");
        exit(1);
    }
    else
    {
        printf("\n\nSuccess connecting\n");
    }


    /*
    pthread_t threads[2];
    void *status;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Spawn the listen/receive deamons
    pthread_create(&threads[0], &attr, sendmessage, (void *)userID);
    pthread_create(&threads[1], &attr, listener, NULL);
	*/
    sendmessage(userID);
    // Keep alive until finish condition is done
    while(!done)
    {
    	
    }

	return 0;
}

bool parseArgument(char *userID, char *serverName, char *argv)
{
	bool success = true;

	printf("Argument: %s\n", argv);

	if (strstr(argv, "-user") != NULL)
	{
		printf("UserID Found!\n");
		strcpy(userID, argv + 5);
		printf("UserID: %s\n", userID);
	}
	else if (strstr(argv, "-server") != NULL)
	{
		printf("Server Name Found!\n");
		strcpy(serverName, argv + 7);
		printf("Server Name: %s\n", serverName);
	}
	else
	{
		success = false;
	}

	return success;
}

void *sendmessage(void *name)
{

    char str[80];
    char msg[100];
    int bufsize=maxx-4;
    char *buffer=malloc(bufsize);
    printf("entered sendmessage\n");
    while(1)
    {
        /*bzero(str,80);
        bzero(msg,100);
        bzero(buffer,bufsize);
        printf("completed bzero\n");*/

        // Get user's message
        fgets(str, 80, stdin);

        // Build the message: "name: message"
        strcpy(msg,name);
        strncat(msg,": \0",100-strlen(str));
        strncat(msg,str,100-strlen(str));

        // Check for quiting
        if(strcmp(str,"exit")==0)
        {

            done = 1;

            // Clean up
            pthread_mutex_destroy(&mutexsum);
            pthread_exit(NULL);
            close(sockfd);
        }    

        // Send message to server
        write(sockfd,msg,strlen(msg));

        // write it in chat window (top)
        printf("%s\n", msg);
    }
}

void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void *listener()
{
    char str[80];
    int bufsize=maxx-4;
    char *buffer=malloc(bufsize);

    while(1)
    {
        bzero(buffer,bufsize);
        //Receive message from server
        read(sockfd,buffer,bufsize);

        //Print on own terminal
        printf("%s", buffer);

        // scroll the top if the line number exceed height
        pthread_mutex_lock (&mutexsum);
        if(line!=maxy/2-2)
            line++;
        pthread_mutex_unlock (&mutexsum);
    }
}