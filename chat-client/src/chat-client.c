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

#define INPUT_LINE 17
#define MAX_MESSAGES 13
#define MAX_MESSAGE_LENGTH 80

// Prototypes
bool parseArgument(char *userID, char *serverName, char *argv);
void *sendmessage(void *name);
void *get_in_addr(struct sockaddr *sa);
void *listener();

char messageQueue[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
int sockfd;
int done = 0;
WINDOW *inputWindow;
WINDOW *logWindow;
int line = 2; // Line position of top
int input = 1; // Line position of top
int maxx,maxy; // Screen dimensions
pthread_mutex_t mutexsum = PTHREAD_MUTEX_INITIALIZER;  


// current IP: 10.113.16.20
int main(int argc, char *argv[])
{
    initscr();
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
        
		printw("UserID: %s\t", userID);
		printw("Server Name: %s\n", serverName);

		// Make socket
	    sockfd = socket(AF_INET, SOCK_STREAM, 0);

	    // attr
	    address.sin_family = AF_INET;
	    address.sin_addr.s_addr = inet_addr(serverName); //127.0.0.1 for local connections
        ///////////////////////////////
	    address.sin_port = htons(9030);
        ///////////////////////////////
	    len = sizeof(address);

	    // Make connection
	    result = connect(sockfd, (struct sockaddr *)&address, len);

	    if(result == -1)
	    {
	        perror("Connection failed, try again.\n");
	    }
	    else
	    {
            write(sockfd,userID,strlen(userID));
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
    endwin();
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

int getOutputPos()
{
    if (line < MAX_MESSAGES)
    {
        line++;
    }
    return line;
}

void printMessages()
{
    for(int i = 0; i < MAX_MESSAGES; i++)
    {
        mvprintw(i+2, 0, "%s", messageQueue[i]);
    }
}

void addMessageToDisplay(char* msg)
{
    memmove(messageQueue[0], messageQueue[1], sizeof(messageQueue) - sizeof(messageQueue[0]));
    strcpy(messageQueue[MAX_MESSAGES - 1], msg);
    printMessages();
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

        // Get user's message
        char newChar = -1;
        int currIndex = 0;
        move(INPUT_LINE,0);
        clrtoeol();
        move(INPUT_LINE,0);
        while (newChar != 10)
        {
            newChar = getch();
            if (newChar == 127)
            {
                str[--currIndex] = 0;                    
            }
            else if (newChar != -1 && currIndex < 80)
            {
                str[currIndex++] = newChar;
            }
            move(INPUT_LINE, 0);
            clrtoeol();  
            mvprintw(INPUT_LINE, 0, "%s", str, currIndex);  

            move(INPUT_LINE, strlen(str));          
        }

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
            bool noSplit = false;
            while ((str[i] != ' ' && i > 0)) 
            {
                if (strlen(str) - i >= 40)
                {
                    noSplit = true;
                    i = 40;
                    break;
                }
                i--;
            }
            if (noSplit)
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
            addMessageToDisplay(msg);

            memset(msg, 0, 100);
            sprintf(msg, "%-16s [%5s] >> %-40s %9s", ip, name, str2, timeString);
            write(sockfd,msg,strlen(msg));  
            addMessageToDisplay(msg);         
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
                sprintf(msg, ">>bye<<%s", (char*)oldName);
                write(sockfd,msg,strlen(msg));
                pthread_mutex_destroy(&mutexsum);
                pthread_exit(NULL);
            }    

            // Send message to server
            write(sockfd,msg,strlen(msg));
            addMessageToDisplay(msg);
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
        //Print on own terminal
        if (!(read(sockfd,buffer,bufsize) < 0) && buffer != NULL)
        {
            int n = send(sockfd, buffer, sizeof(buffer), MSG_NOSIGNAL);
            if (n == -1)
            {
                addMessageToDisplay("The connestion with the server has been broken. No messages can be sent.");
                close(sockfd);
                done = 1;
                break;
            }
            else
            {
                if (buffer[0] != 0)
                {
                    addMessageToDisplay(buffer);
                }                
            }            
        }
        // scroll the top if the line number exceed height
        //pthread_mutex_lock (&mutexsum);
/*        if(line!=maxy/2-2)
            line++;*/
        //pthread_mutex_unlock (&mutexsum);
    }
}