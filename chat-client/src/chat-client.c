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

//Includes add by Attila
#include <ncurses.h>
#include <fcntl.h>
#include <stdarg.h>

// Constants
#define CURR_SOCKET 9030
#define INPUT_LINE 17
#define MAX_MESSAGES 13
#define MAX_MESSAGE_LENGTH 80

//Constants by Attila
#define CHAT_HEIGHT 5
#define CHAT_WIDTH COLS-2
#define CHAT_START_X 1
#define CHAT_START_Y LINES-5

#define MESSAGE_HEIGHT LINES-4
#define MESSAGE_WIDTH COLS
#define MESSAGE_START_X 0
#define MESSAGE_START_Y 0

// Prototypes
void getIP(char *ip);
void getTime(char* timeString);
void getUserInput(char* str);
void printHeader();
void addMessageToDisplay(char* msg);
int getOutputPos();
void setUserName(char* str, char* name);
void printMessages();
void splitMessage(char* str, char* str1, char* str2);
void sendMessage(int socketID, char* ip, char* name, char* str, char* timeString);
void *getSendMessage();
void *listener();
bool parseArgument(char *argv);

// Globals
char gUserName[20], gServerName[20];
char messageQueue[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
int sockfd;
int done = 0;
int line = 2; // Line position of messages
pthread_mutex_t mutexsum = PTHREAD_MUTEX_INITIALIZER;  

//ATTTILAS GLOBALS
WINDOW* chatWindow;
WINDOW* messageWindow;

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

void getUserInput(char* str)//EDITED HERE BY ATTILA
{
    char newChar = -1;
    int currIndex = 0;
    //move(INPUT_LINE,0); removed by Attila
    wmove(chatWindow, 2, 3);
    //clrtoeol(); removed by Attila
    wclrtoeol(chatWindow);

    //move(INPUT_LINE,0); removed by Attila
    wmove(chatWindow, 2, 3);
    box(chatWindow, 0, 0);   
    mvwprintw(chatWindow, 2, 2, ">", currIndex); 

    flushinp();
    while (newChar != 10 || currIndex == 0)
    {
        newChar = wgetch(chatWindow);

        //move(INPUT_LINE + 1, 0); removed by Attila
        wmove(chatWindow, 2, 3);

        //clrtoeol(); removed by Attila
	wclrtoeol(chatWindow); 
	mvwprintw(chatWindow, 2, 2, ">", currIndex); 
        if (newChar == 127)
        {
            str[--currIndex] = 0;                    
        }
        else if (newChar != -1 && currIndex < 80 && newChar > 31 && newChar < 127)
        {
            str[currIndex++] = newChar;
        }
        //move(INPUT_LINE, 0); removed by Attila
        wmove(chatWindow,2, 3);

        //clrtoeol(); removed by Attila
	wclrtoeol(chatWindow);

        //mvprintw(INPUT_LINE, 0, "%s", str, currIndex); removed by Attila
	mvwprintw(chatWindow, 2, 3, "%s", str, currIndex); 

        //move(INPUT_LINE, strlen(str));removed by Attila
	wmove(chatWindow, 2, strlen(str) + 3);  
	box(chatWindow, 0, 0);   
    }
}

void printHeader()
{
    move(0, 0);
    clrtoeol();
    printw("UserID: %-20s\tServer Name: %s\n", gUserName, gServerName);
}

void addMessageToDisplay(char* msg)
{
    memmove(messageQueue[0], messageQueue[1], sizeof(messageQueue) - sizeof(messageQueue[0]));
    sprintf(msg,"%-80s",msg);
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

void setUserName(char* str, char* name)
{
    if (strlen(&str[6]) > 20)
    {
        addMessageToDisplay("/name has a maximum name size of 20 characters, try again.");                    
    }
    else if (strlen(&str[6]) > 0)
    {
        write(sockfd,str,strlen(str));
        //strcpy(gUserName, &str[6]);
        strncpy(name, gUserName, 5);
        printHeader();
    }
    else
    {
        addMessageToDisplay("Invalid username, try again.");
    }
}

void printMessages() //EDITED HERE BY ATTILA
{
    int x, y;
    getyx(chatWindow, y, x);

    for(int i = 0; i < MAX_MESSAGES; i++)
    {
        //move(i+2, 0); Removed by Attila	
	//wmove(messageWindow, i+2, 1);

        //clrtoeol(); Removed by Attila
	//wclrtoeol(messageWindow); 

        //mvprintw(i+2, 0, "%s", messageQueue[i]); Removed by Attila
	mvwprintw(messageWindow,i+2, 1, "%s", messageQueue[i]);
    }
    //refresh(); Removed by Attila
    //wrefresh(messageWindow);

    //move(INPUT_LINE, 0); Removed by Attila
    //return to getxy
    wmove(chatWindow, y, x);
    box(chatWindow,0,0);
    box(messageWindow,0,0);
    wrefresh(messageWindow);
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

void endGet()
{
    done = 1;
    write(sockfd,">>bye<<",strlen(">>bye<<"));
    pthread_mutex_destroy(&mutexsum);
    pthread_exit(NULL);
}

void interpretCommand(char* str, char* name)
{
    if(strcmp(str,"/users")==0)
    {
        write(sockfd,str,strlen(str));
    }    
    else if(strstr(str,"/name ") != NULL)
    {
        setUserName(str, name);
    }    
    else if(strcmp(str, "/clear") == 0)
    {
        memset(messageQueue, 0, sizeof(messageQueue));
        printMessages();
    }
    else if(strcmp(str, "/exit") == 0)
    {
        endGet();
    }
    else if(strcmp(str, "/?") == 0)
    {
        addMessageToDisplay("Valid Commands: /users | /clear | /name <new-name> | /? | /exit");
    }
    else
    {
        addMessageToDisplay("Invalid Command: type /? for a list of commands");
    }    
}

void *getSendMessage()
{
    char str[81], str1[41], str2[41], ip[20], name[5], timeString[11];
    strncpy(name, gUserName, 5);
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

        if(strcmp(str,">>bye<<")==0)
        {
            endGet();
        }    
        if (str[0] == '/')
        {
            interpretCommand(str, name);
        }
        else if (strlen(str) > 40)
        {   
            splitMessage(str, str1, str2);
            sendMessage(sockfd, ip, name, str1, timeString);
            sendMessage(sockfd, ip, name, str2, timeString);  
        }
        else
        {
            sendMessage(sockfd, ip, name, str, timeString);                
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

bool parseArgument(char *argv)
{
    bool success = true;
    if (strstr(argv, "-user") != NULL)
    {
        strncpy(gUserName, argv + 5, 20);
    }
    else if (strstr(argv, "-server") != NULL)
    {
        strcpy(gServerName, argv + 7);
    }
    else
    {
        success = false;
    }

    return success;
}

WINDOW* newWindow(int height, int width, int y, int x)
{
    WINDOW* win;
    win = newwin(height, width, y, x);
    box(win,0,0);
    wmove(win,1,1);
    wrefresh(win);
    
    return win;
}

int main(int argc, char *argv[])
{
    initscr();
    keypad(chatWindow, TRUE);     
    int len;
    int result;  
    //Added by Attila below    
    initscr();
    cbreak();
    noecho();
    refresh();  
    
    messageWindow = newWindow(MESSAGE_HEIGHT,MESSAGE_WIDTH,MESSAGE_START_Y, MESSAGE_START_X);
    scrollok(messageWindow, TRUE);

    chatWindow = newWindow(CHAT_HEIGHT, CHAT_WIDTH, CHAT_START_Y, CHAT_START_X);
    scrollok(chatWindow,TRUE);

    if ((argc == 3 && (!parseArgument(argv[1]) || !parseArgument(argv[2]))) || argc != 3)
    {
        printf("Proper Usage: chat-client -user<userID> -server<server name>\n");
    }
    else
    {
        
        printHeader();

        // Make socket
        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        // attr
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = inet_addr(gServerName); //127.0.0.1 for local connections
        address.sin_port = htons(CURR_SOCKET);
        
        // Make connection
        if(connect(sockfd, (struct sockaddr *)&address, sizeof(address)) == -1)
        {
            perror("Connection failed, try again.\n");
        }
        else
        {
            write(sockfd,gUserName,strlen(gUserName));
            pthread_t threads[2];
            void *status;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

            // Spawn the listen/receive deamons
            pthread_create(&threads[0], &attr, getSendMessage, (void *)gUserName);
            pthread_create(&threads[1], &attr, listener, NULL);
            while(!done);
        }
    }   
    delwin(chatWindow);//Edited by Attila, used to be just endwin();
    delwin(messageWindow);
    return 0;
}
