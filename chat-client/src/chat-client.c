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

#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <time.h>
#include <signal.h>

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
	char userID[20], serverName[30];
	int len;
    int result;
    char buf[256];
    struct sockaddr_in address;

	if ((argc == 3 && (!parseArgument(userID, serverName, argv[1]) || !parseArgument(userID, serverName, argv[2]))) || argc != 3)
	{
		printf("Proper Usage: chat-client -user<userID> -server<server name>\n");
	}
	else
	{
		printf("UserID: %s\n", userID);
		printf("Server Name: %s\n", serverName);

		// Make socket
	    sockfd = socket(AF_INET, SOCK_STREAM, 0);

	    // attr
	    address.sin_family = AF_INET;
	    address.sin_addr.s_addr = inet_addr("127.0.0.1");
	    address.sin_port = htons(9037);

	    len = sizeof(address);

	    // Make connection
	    result = connect(sockfd, (struct sockaddr *)&address, len);

	    if(result == -1)
	    {
	        perror("Connection failed, try again.\n");
	        exit(1);
	    }
	    else
	    {
    	    pthread_t threads[2];
		    void *status;
		    pthread_attr_t attr;
		    pthread_attr_init(&attr);
		    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		    // Spawn the listen/receive deamons
		    pthread_create(&threads[0], &attr, sendmessage, (void *)userID);
		    pthread_create(&threads[1], &attr, listener, NULL);
		    while(!done);
	    }

	    //sendmessage(userID);
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
    // Keep alive until finish condition is done
	return 0;
}

bool parseArgument(char *userID, char *serverName, char *argv)
{
	bool success = true;
	if (strstr(argv, "-user") != NULL)
	{
		strcpy(userID, argv + 5);
	}
	else if (strstr(argv, "-server") != NULL)
	{
		strcpy(serverName, argv + 7);
	}
	else
	{
		success = false;
	}

	return success;
}

void *sendmessage(void *oldName)
{
    char name[5];
    strncpy(name, oldName, 5);
    char str[81];
    char ip[20];
    char timeString[11];
    char msg[100];
    int bufsize=maxx-4;
    char *buffer=malloc(bufsize);
    char str1[41];    
    char str2[41];
    while(!done)
    {
        memset(str, 0, 80);
        memset(str1, 0, 41);
        memset(str2, 0, 41);
        memset(msg, 0, 100);
        memset(ip, 0, 20);
        memset(timeString, 0, 20);
        /*bzero(str,80);
        bzero(msg,100);
        bzero(buffer,bufsize);
        printf("completed bzero\n");*/

        // Get user's message
        fgets(str, 81, stdin);
        int c;        

        //https://stackoverflow.com/questions/1570511/c-code-to-get-the-ip-address
        int fd;
        struct ifreq ifr;
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        ifr.ifr_addr.sa_family = AF_INET;
        snprintf(ifr.ifr_name, IFNAMSIZ, "ens33");
        ioctl(fd, SIOCGIFADDR, &ifr);
        /* and more importantly */
        sprintf(ip, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
        close(fd);
        //printf("%s\n", ip);

        // Time
        time_t currTime;
        struct tm* currTimeInfo;
        char formattedTimeString[28];

        // Get the local time in the tm struct
        time(&currTime);
        currTimeInfo = localtime(&currTime);

        // Format the time to the expected style
        strftime(timeString, 11, "(%H:%M:%S)", currTimeInfo);

        char *current_pos = strchr(str,'\n');
        while (current_pos)
        {
            *current_pos = ' ';
            current_pos = strchr(current_pos,'\n');
        }

        if (strlen(str) > 41)
        {   
            int i = 40;
            bool isTooBig = false;
            if (strlen(str) == 80)
            {
                while ((c = fgetc(stdin)) != '\n' && c != EOF);
            }
            if (strlen(str) > 40)
            {
                while ((str[i] != ' ' && i > 0)) 
                {
                    if (strlen(str) - i <= 40)
                    {
                        isTooBig = true;
                        i = 40;
                        break;
                    }
                    i--;
                }
            }
            if (isTooBig)
            {
                strncpy(str1, str, i);
                strncpy(str2, str + i, strlen(str) - i);
            }
            else
            {
                strncpy(str1, str, i);
                strncpy(str2, str + i + 1, strlen(str) - i);
                str2[strlen(str2) - 1] = 0;
            }


            // Build the message: "name: message"
            memset(msg, 0, 100);

            sprintf(msg, "%-16s [%5s] >> %-40s %9s", ip, name, str1, timeString);
            write(sockfd,msg,strlen(msg));
            printf("%s\n", msg); 

            memset(msg, 0, 100);
            sprintf(msg, "%-16s [%5s] >> %-40s %9s", ip, name, str2, timeString);
            write(sockfd,msg,strlen(msg));  
            printf("%s\n", msg);           
        }
        else
        {
            str[strlen(str) - 1] = 0;
            memset(msg, 0, 100);
            sprintf(msg, "%-16s [%5s] >> %-40s %9s", ip, name, str, timeString);

            // Check for quiting
            if(strcmp(str,">>bye<<")==0)
            {
                done = 1;
                // Clean up
                pthread_mutex_destroy(&mutexsum);
                pthread_exit(NULL);
            }    

            // Send message to server
            write(sockfd,msg,strlen(msg));
            printf("%s\n", msg); 
        }

        // write it in chat window (top)
               
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
    int bufsize=80;
    char *buffer=malloc(bufsize);
    while(!done)
    {
        memset(buffer,0,80);
        //Receive message from server
        read(sockfd,buffer,bufsize);

        //Print on own terminal
        if (buffer != NULL)
        {
            printf("%s\n", buffer);    
        }
        // scroll the top if the line number exceed height
        //pthread_mutex_lock (&mutexsum);
/*        if(line!=maxy/2-2)
            line++;*/
        //pthread_mutex_unlock (&mutexsum);
    }
}