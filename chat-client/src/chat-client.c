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

// Constants
#define CURR_SOCKET 9030
#define INPUT_LINE 17
#define MAX_MESSAGES 13
#define MAX_MESSAGE_LENGTH 80

// Prototypes
void getIP(char *ip);
void getTime(char* timeString);
void getUserInput(char* str);
void addMessageToDisplay(char* msg);
int getOutputPos();
void printMessages();
void splitMessage(char* str, char* str1, char* str2);
void sendMessage(int socketID, char* ip, char* name, char* str, char* timeString);
void *getSendMessage(void *oldName);
void *listener();
bool parseArgument(char *userID, char *serverName, char *argv);

// Globals
char messageQueue[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
int sockfd;
int done = 0;
int line = 2; // Line position of messages
pthread_mutex_t mutexsum = PTHREAD_MUTEX_INITIALIZER;  


void getIP(char *ip)
{
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
}

void getTime(char* timeString)
{
    time_t currTime;
    struct tm* currTimeInfo;
    time(&currTime);
    currTimeInfo = localtime(&currTime);
    strftime(timeString, 11, "(%H:%M:%S)", currTimeInfo);
}

void getUserInput(char* str)
{
    char newChar = -1;
    int currIndex = 0;
    move(INPUT_LINE,0);
    clrtoeol();
    move(INPUT_LINE,0);
    flushinp();
    while (newChar != 10 || currIndex == 0)
    {
        newChar = getch();
        move(INPUT_LINE + 1, 0);
        clrtoeol();  
        if (newChar == 7)
        {
            str[--currIndex] = 0;                    
        }
        else if (newChar != -1 && currIndex < 80 && newChar > 31 && newChar < 127)
        {
            str[currIndex++] = newChar;
        }
        move(INPUT_LINE, 0);
        clrtoeol();  
        mvprintw(INPUT_LINE, 0, "%s", str, currIndex);  

        move(INPUT_LINE, strlen(str));          
    }
}

void addMessageToDisplay(char* msg)
{
    memmove(messageQueue[0], messageQueue[1], sizeof(messageQueue) - sizeof(messageQueue[0]));
    strcpy(messageQueue[MAX_MESSAGES - 1], msg);
    printMessages();
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
        move(i+2, 0);
        clrtoeol();
        mvprintw(i+2, 0, "%s", messageQueue[i]);
    }
    refresh();
    move(INPUT_LINE, 0);
}

void splitMessage(char* str, char* str1, char* str2)
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
    }
}

void sendMessage(int socketID, char* ip, char* name, char* str, char* timeString)
{
    char msg[80];
    memset(msg, 0, 80);
    sprintf(msg, "%-16s [%-5s] >> %-40s %9s", ip, name, str, timeString);
    write(socketID,msg,strlen(msg));
    addMessageToDisplay(msg);    
}

void *getSendMessage(void *oldName)
{
    char str[81], str1[41], str2[41], ip[20], name[5], timeString[11];
    strncpy(name, oldName, 5);
    while(!done)
    {
        memset(str, 0, 80);
        memset(ip, 0, 20);
        memset(timeString, 0, 20);
        memset(str1, 0, 41);
        memset(str2, 0, 41);

        // Get user's message

        getUserInput(str);
        getIP(ip);
        getTime(timeString);

        if (strlen(str) > 40)
        {   
            splitMessage(str, str1, str2);
            sendMessage(sockfd, ip, name, str1, timeString);
            sendMessage(sockfd, ip, name, str2, timeString);  
        }
        else
        {
            if(strcmp(str,">>bye<<")==0)
            {
                done = 1;
                memset(str, 0, 80);
                sprintf(str, ">>bye<<%s", (char*)oldName);
                write(sockfd,str,strlen(str));
                pthread_mutex_destroy(&mutexsum);
                pthread_exit(NULL);
            }    
            else if(strcmp(str,"/users")==0)
            {
                write(sockfd,str,strlen(str));
            }    
            else
            {
                sendMessage(sockfd, ip, name, str, timeString);                
            }
        }
    }
}

void *listener()
{
    char str[80];
    int bufsize=80;
    char *buffer=malloc(bufsize);
    while(!done)
    {
        memset(buffer,0,80);
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
                if (buffer[0] != 0 && strlen(buffer) > 7)
                {
                    addMessageToDisplay(buffer);
                }                
            }            
        }
    }
}

bool parseArgument(char *userID, char *serverName, char *argv)
{
    bool success = true;
    if (strstr(argv, "-user") != NULL)
    {
        strncpy(userID, argv + 5, 20);
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

int main(int argc, char *argv[])
{
    initscr();
    keypad(stdscr, TRUE); 
    char userID[20], serverName[30];
    int len;
    int result;    

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
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(serverName); //127.0.0.1 for local connections
        address.sin_port = htons(CURR_SOCKET);
        
        // Make connection
        if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
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
            pthread_create(&threads[0], &attr, getSendMessage, (void *)userID);
            pthread_create(&threads[1], &attr, listener, NULL);
            while(!done);
        }
    }   
    endwin();
    return 0;
}