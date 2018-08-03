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

#include <arpa/inet.h>
#include <curses.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// Constants
#define SOCKET_NUMBER       9030

#define MAX_MESSAGES        10
#define MAX_MESSAGE_LENGTH  80
#define HALF_MESSAGE_LENGTH MAX_MESSAGE_LENGTH / 2
#define MIN_MESSAGE_LENGTH  7
#define MIN_MSG_RECV_LENGTH 13
#define BACKSPACE_KEY       7
#define ENTER_KEY           10
#define MAX_KEY_VALUE       127
#define MIN_KEY_VALUE       31

#define TIME_STRING_LENGTH  11
#define IP_STRING_LENGTH    20
#define IP_CONNECTION_TYPE  "ens33"

#define NAME_MAX_CHARACTERS 15
#define NAME_CUTOFF         5

#define INCOMING_INPUT_LINE 2
#define INPUT_INDENT        (COLS / 2) - (MAX_MESSAGE_LENGTH / 4)

#define TITLES_HEIGHT       3
#define TOP_LEFT            0

#define WINDOW_WIDTH        COLS
#define WINDOW_START_X      0

#define OUTGOING_HEIGHT     6
#define OUTGOING_START_Y    LINES-6

#define INCOMING_HEIGHT     LINES - (TITLES_HEIGHT * 2) - OUTGOING_HEIGHT
#define INCOMING_START_Y    OUTGOING_START_Y - TITLES_HEIGHT

#define WHITE_BACKGROUND    1
#define BLACK_BACKGROUND    2

#define OUT_TITLE_TEXT      "Outgoing Message"
#define OUT_BAR_MAX_TEXT    "Max message size reached!"
#define MESSAGE_INDICATOR   ">"
#define DISCONNECT_MESSAGE  "The connection with the server has been broken. No messages can be sent."
#define INVALID_COMMAND_MSG "Invalid Command: type /? for a list of commands"
#define HELP_COMMAND_MSG    "Valid Commands: /users | /clear | /name <new-name> | /? | /exit"

// Prototypes
void getIP(char*);
void getTime(char*);
void getUserInput(char*);
char getCharacterInput(char*);
void printCurrentUserInput(char*);
void printHeader(char*);
void addMessageToDisplay(char*);
void setUserName(char*, char*);
void printMessages();
void splitMessage(char*, char*, char*);
void sendMessage(char*, char*, char*, char*);
void endGet();
void interpretCommand(char*, char*);
void *getUserMessage();
void *getServerMessage();
bool parseArgument(char *);
WINDOW* newWindow(int, int, int, int, int);
void clearWindow(WINDOW*);
void printIntro();
void intializeColors();
void initalizeWindows();
int initalizeAndConnectToSocket();
void spawnThreads();
void pauseAndCleanWindows();

// Global Windows
WINDOW* outgoingTitleWindow;
WINDOW* outgoingMessageWindow;
WINDOW* incomingTitleWindow;
WINDOW* incomingMessageWindow;

// Global Variables
char gUserName[NAME_MAX_CHARACTERS] = {0}, gServerName[IP_STRING_LENGTH] = {0};
char gMessageQueue[MAX_MESSAGES][MAX_MESSAGE_LENGTH];
int  gSocketID;
bool done = false;

void getIP(char *ip)
{
    // FUNCTION     : getIP
    // DESCRIPTION  : This function gets the current IP of the machine and saves it to the given char pointer
    // PARAMETERS   :   
    //    char *ip      : The destination for the IP string
    // RETURNS      : 
    //    VOID
    
    //https://stackoverflow.com/questions/1570511/c-code-to-get-the-ip-address
    int tempSocket;
    struct ifreq ifr;

    tempSocket = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    snprintf(ifr.ifr_name, IFNAMSIZ, IP_CONNECTION_TYPE);
    ioctl(tempSocket, SIOCGIFADDR, &ifr);

    sprintf(ip, "%s", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
    close(tempSocket);
}

void getTime(char* timeString)
{
    // FUNCTION     : getTime
    // DESCRIPTION  : This function gets the current local time and saves it to the given char pointer
    // PARAMETERS   :   
    //    char *timeString      : The destination for the time string
    // RETURNS      : 
    //    VOID
    
    time_t currTime;
    struct tm* currTimeInfo;
    time(&currTime);
    currTimeInfo = localtime(&currTime);
    strftime(timeString, TIME_STRING_LENGTH, "(%H:%M:%S)", currTimeInfo);
}

void getUserInput(char* str)
{
    // FUNCTION     : getUserInput
    // DESCRIPTION  : This function takes the user's input, and populates the given char* with
    //                the results once the user presses enter to send the message
    // PARAMETERS   :   
    //    char* str     : The pointer to the string functioning as an output
    // RETURNS      : 
    //    VOID
    
    int currentIndex = 0;

    // Flush the input buffer, clear the window, print the message indicator, and move the cursor to the start position
    flushinp();
    clearWindow(outgoingMessageWindow);
    mvwprintw(outgoingMessageWindow, INCOMING_INPUT_LINE, INPUT_INDENT - (strlen(MESSAGE_INDICATOR) + 1), MESSAGE_INDICATOR);  
    wmove(outgoingMessageWindow, INCOMING_INPUT_LINE, INPUT_INDENT);

    // Get the input a character at a time, comparing to the ENTER_KEY const each time
    while ((getCharacterInput(str) != ENTER_KEY || strlen(str) == 0) && !done)
    {   
        // clear the window and reprint the indicator
        clearWindow(outgoingMessageWindow);
        mvwprintw(outgoingMessageWindow, INCOMING_INPUT_LINE, INPUT_INDENT - (strlen(MESSAGE_INDICATOR) + 1), MESSAGE_INDICATOR);  
        
        // Reprint the string so far
        printCurrentUserInput(str);
    }
}

char getCharacterInput(char* str)
{
    // FUNCTION     : getCharacterInput
    // DESCRIPTION  : This function takes a single character of the user's input, and manipulates the 
    //                given string appropriately
    // PARAMETERS   : 
    //    char* str         : The pointer to the in-process string
    //    int* currentIndex : The pointer index iterator, to see how long the string is
    // RETURNS      : 
    //    char          : The character the user input

    // Get the keystroke from the user and reprint the header text     
    char newChar = wgetch(outgoingMessageWindow);    
    printHeader(OUT_TITLE_TEXT);

    if (newChar == BACKSPACE_KEY && strlen(str) > 0)
    {   
        // If the user presses backspace and there are characters in the str, decrement currentIndex and clear the last character
        str[strlen(str) - 1] = 0;                    
    }
    else if (strlen(str) < MAX_MESSAGE_LENGTH && newChar > MIN_KEY_VALUE && newChar < MAX_KEY_VALUE)
    {
        // If the key is valid and there's space in the string, increment currentIndex and add newChar to the string
        str[strlen(str)] = newChar;
    }
    else if (strlen(str) >= MAX_MESSAGE_LENGTH)
    {
        // If the currentIndex is at the max message size, print the max size warning in the header
        printHeader(OUT_BAR_MAX_TEXT);
    }
    return newChar;
}

void printCurrentUserInput(char* str)
{
    // FUNCTION     : printCurrentUserInput
    // DESCRIPTION  : This function takes the user's input, and populates the given char* with
    //                the results once the user presses enter to send the message
    // PARAMETERS   :   
    //    char* str     : The pointer to the string functioning as an output
    // RETURNS      : 
    //    VOID

    if (strlen(str) > HALF_MESSAGE_LENGTH)
    {           
        // Declare str1 and str2, which will be populates with either half of the gven string
        char str1[HALF_MESSAGE_LENGTH + 1], str2[HALF_MESSAGE_LENGTH + 1];
        memset(str1, 0, HALF_MESSAGE_LENGTH + 1);
        memset(str2, 0, HALF_MESSAGE_LENGTH + 1);

        // Split and print the two string
        splitMessage(str, str1, str2);
        mvwprintw(outgoingMessageWindow, INCOMING_INPUT_LINE, INPUT_INDENT, "%s", str1);  
        mvwprintw(outgoingMessageWindow, INCOMING_INPUT_LINE + 1, INPUT_INDENT, "%s", str2);  
    }
    else
    {
        // print the entire string, as it isn't past the single message barrier
        mvwprintw(outgoingMessageWindow, INCOMING_INPUT_LINE, INPUT_INDENT, "%s", str);  
    }
}

void printHeader(char* otwTitle)
{
    // FUNCTION     : printHeader
    // DESCRIPTION  : This function spawns the getUserMessage and getServerMessage threads
    // PARAMETERS   :   
    //    char* otwTitle    : The message to display in the outgoingTitleWindow (otw)
    // RETURNS      : 
    //    VOID
    
    // Clear the title windows
    clearWindow(incomingTitleWindow);
    clearWindow(outgoingTitleWindow);

    // Find the title offset where the message is centered and print to that location in both windows
    int xOffset = (COLS - (strlen("Username: ") + strlen(gUserName)))/2 + 1;
    mvwprintw(incomingTitleWindow, TITLES_HEIGHT/2, xOffset, "Username: %s", gUserName);
    xOffset = (COLS - strlen(otwTitle))/2 + 1;
    mvwprintw(outgoingTitleWindow, TITLES_HEIGHT/2, xOffset, otwTitle);

    // Refresh all title windows
    wrefresh(incomingTitleWindow);
    wrefresh(outgoingTitleWindow);
}

void addMessageToDisplay(char* msg)
{
    // FUNCTION     : addMessageToDisplay
    // DESCRIPTION  : This function adds a message to the gMessageQueue and reprints
    //                the message list screen
    // PARAMETERS   :   
    //    char* msg     : The message to add to the gMessageQueue for displaying
    // RETURNS      : 
    //    VOID
    
    // If the message is not a repeat of the previous message and it's longer than MIN_MSG_RECV_LENGTH characters, add it to the queue
    if (strcmp(gMessageQueue[MAX_MESSAGES - 1], msg) != 0 && strlen(msg) > MIN_MSG_RECV_LENGTH)
    {
        // shift the queue up one to simulate scrolling upwards, save to the new end of the queue, and reprint
        memmove(gMessageQueue[0], gMessageQueue[1], sizeof(gMessageQueue) - sizeof(gMessageQueue[0]));
        strcpy(gMessageQueue[MAX_MESSAGES - 1], msg);    
        printMessages();
    }
}

void setUserName(char* str, char* name)
{
    // FUNCTION     : setUserName
    // DESCRIPTION  : This function changes the user's username to the new given name
    // PARAMETERS   :   
    //    char* str     : The message from the user including the '/name ' command
    //    char* name    : The shortened name to change for outbound messages
    // RETURNS      : 
    //    VOID
    
    char strOffset = strlen("/name ");  // the offset caused by the command to get the actual desired name
    if (strlen(&str[strOffset]) > NAME_MAX_CHARACTERS)
    {
        // Print this if the name is over NAME_MAX_CHARACTERS (15) characters long
        addMessageToDisplay("/name has a maximum name size of 15 characters, try again.");                    
    }
    else if (strlen(&str[strOffset]) > 0)
    {        
        write(gSocketID,str,strlen(str));       // Send a message to the server to declare the name change
        strcpy(gUserName, &str[strOffset]);     // Copy the full username to the global gUserName
        strncpy(name, gUserName, NAME_CUTOFF);  // Copy the first NAME_CUTOFF characters into the name string
    }
    else
    {
        // If the name is for some reason invalid (i.e. 0 characters long)
        addMessageToDisplay("Invalid username, try again.");
    }
}

void printMessages()
{
    // FUNCTION     : printMessages
    // DESCRIPTION  : This function prints all messages in the gMessageQueue to the screen
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    // Get the current x/y coordinates of the cursor to return them at the end of the print
    int x, y;
    getyx(outgoingMessageWindow, y, x);
    clearWindow(incomingMessageWindow);

    for(int i = 0; i < MAX_MESSAGES; i++)
    {
        // Print all of the messages centered in the console
        int xOffset = (COLS - strlen(gMessageQueue[i]))/2;
        int yOffset = LINES - (TITLES_HEIGHT * 2) - OUTGOING_HEIGHT - MAX_MESSAGES - 1; // -1 because of the box border
        mvwprintw(incomingMessageWindow, i + yOffset, xOffset, "%s", gMessageQueue[i]);
    }    

    // Reprint the header and refresh the message windows
    printHeader(OUT_TITLE_TEXT);
    wrefresh(incomingMessageWindow);
    wrefresh(outgoingMessageWindow);

    // Move the cursor back to the coordinates in the outgoingMessageWindow for user input purposes
    wmove(outgoingMessageWindow, y, x);
}

void splitMessage(char* str, char* str1, char* str2)
{
    // FUNCTION     : splitMessage
    // DESCRIPTION  : This function spawns the getUserMessage and getServerMessage threads
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    int i = HALF_MESSAGE_LENGTH;
    bool noSplit = false;

    while ((str[i] != ' ' && i > 0)) 
    {
        if (strlen(str) - i >= HALF_MESSAGE_LENGTH)
        {
            noSplit = true;
            i = HALF_MESSAGE_LENGTH;
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

void sendMessage(char* ip, char* name, char* str, char* timeString)
{
    // FUNCTION     : sendMessage
    // DESCRIPTION  : This function takes the components of a message to the server and
    //                amalgomates them into a single set-sized message and sends it to 
    //                the server and adds it to the display
    // PARAMETERS   :   
    //    char* ip          : The string containing the IP of the client
    //    char* name        : The string containing the name of the client
    //    char* str         : The string containing the message from the user
    //    char* timeString  : The string containing the current time
    // RETURNS      : 
    //    VOID

    // Initialize msg
    char msg[MAX_MESSAGE_LENGTH];
    memset(msg, 0, MAX_MESSAGE_LENGTH);

    // Populate msg with the formatted string
    sprintf(msg, "%-16s [%-5s] >> %-40s %9s", ip, name, str, timeString);

    // Write msg to the server, and the client display
    write(gSocketID,msg,strlen(msg));
    addMessageToDisplay(msg);    
}

void endGet()
{
    // FUNCTION     : endGet
    // DESCRIPTION  : This function ends the current thread, sets the program to done, and 
    //                clears the outgoingMessageWindow
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    pthread_mutex_t mutexsum = PTHREAD_MUTEX_INITIALIZER;  
    done = 1;
    write(gSocketID,">>bye<<",strlen(">>bye<<"));
    clearWindow(outgoingMessageWindow);
    pthread_mutex_destroy(&mutexsum);
    pthread_exit(NULL);
}

void interpretCommand(char* str, char* name)
{
    // FUNCTION     : interpretCommand
    // DESCRIPTION  : This function intereprets any commands given by the user locally
    // PARAMETERS   :   
    //    char* str     : The string containing the command and any arguments the command might take
    //    char* name    : The pointer to the name, to be changed if the command is /name
    // RETURNS      : 
    //    VOID

    if(strcmp(str,"/users") == 0)
    {
        // Send "/users" to the server, where it will reply with the currently active users
        write(gSocketID,str,strlen(str));
    }    
    else if(strstr(str,"/name ") == str)
    {
        // set the username to the new given one
        setUserName(str, name);
    }    
    else if(strcmp(str, "/clear") == 0)
    {
        // clear the message queue, and thereby clear the screen
        memset(gMessageQueue, 0, sizeof(gMessageQueue));
        printMessages();
    }
    else if(strcmp(str, "/exit") == 0)
    {
        // End the program, equivilent to ">>bye<<"
        endGet();
    }
    else if(strcmp(str, "/?") == 0 || strcmp(str, "/help") == 0)
    {
        // Display the help message, which conatins all current commands
        addMessageToDisplay(HELP_COMMAND_MSG);
    }
    else
    {
        // invalid / unrecognised command, print the invalid command message
        addMessageToDisplay(INVALID_COMMAND_MSG);
    }    
}

void *getUserMessage()
{
    // FUNCTION     : getUserMessage
    // DESCRIPTION  : This threaded function gets the user's message input, splits and parcels the 
    //                message, and sends it to the server for distribution
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    void *    : no return, requirement of a thread

    char str[MAX_MESSAGE_LENGTH], str1[HALF_MESSAGE_LENGTH + 1], str2[HALF_MESSAGE_LENGTH + 1], ip[IP_STRING_LENGTH], name[NAME_CUTOFF], timeString[TIME_STRING_LENGTH];
    
    // Take the first NAME_CUTOFF characters from gUserName to serve as the name included in the message
    strncpy(name, gUserName, NAME_CUTOFF);

    // clear the ip string and populate it with the current (unchanging) ip address
    memset(ip, 0, IP_STRING_LENGTH);
    getIP(ip);  
    
    while(!done)
    {
        // Clear the strings
        memset(timeString, 0, TIME_STRING_LENGTH);
        memset(str, 0, MAX_MESSAGE_LENGTH);
        memset(str1, 0, HALF_MESSAGE_LENGTH + 1);
        memset(str2, 0, HALF_MESSAGE_LENGTH + 1);

        // Get user's message and the time once the message has been gathered
        getUserInput(str);
        getTime(timeString);

        // If the program lost connection to the server, don't try to send the message
        if (!done)
        {
            if(strcmp(str,">>bye<<")==0)    // Assignment mandated exit string
            {
                endGet();
            }    
            else if (str[0] == '/')  // If the string begins with the '/' that means its a command
            {
                // Interpret that command
                interpretCommand(str, name);
            }
            else if (strlen(str) > HALF_MESSAGE_LENGTH) // If the message needs to be split into two messages
            {   
                // Split and send both messages at once
                splitMessage(str, str1, str2);
                sendMessage(ip, name, str1, timeString);
                sendMessage(ip, name, str2, timeString);  
            }
            else
            {
                // Otherwise send the whole typed message
                sendMessage(ip, name, str, timeString);                
            }
        }
    }
}

void *getServerMessage()
{
    // FUNCTION     : getServerMessage
    // DESCRIPTION  : This threaded function confirms that the socket is still connected to the server and
    //                also recieves any messages sent by the server and displays them to the screen
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    void *    : no return, requirement of a thread

    char *buffer=malloc(MAX_MESSAGE_LENGTH);

    // Run while the signal says the other threads are running
    while(!done)
    {
        memset(buffer,0,MAX_MESSAGE_LENGTH);
        if (!(read(gSocketID,buffer,MAX_MESSAGE_LENGTH) < 0) && buffer != NULL)
        {   
            // Send a test signal to confirm that the connection is still valid
            if (send(gSocketID, buffer, MAX_MESSAGE_LENGTH, MSG_NOSIGNAL) == -1)
            {   
                // The connection has been broken, close the thread and signal the completion of the program
                addMessageToDisplay(DISCONNECT_MESSAGE);
                close(gSocketID);
                done = true;
                break;
            }
            else
            {
                // Confirming the message isnt a junk message read accidentally
                if (buffer[0] != 0 && strlen(buffer) > MIN_MESSAGE_LENGTH)
                {
                    addMessageToDisplay(buffer);
                }                
            }            
        }
    }
}

bool parseArgument(char *argv)
{
    // FUNCTION     : parseArgument
    // DESCRIPTION  : This function parses the command line arguments
    // PARAMETERS   :   
    //    char *argv    : The array of command line arguments given
    // RETURNS      : 
    //    bool          : If the command was valid or not

    bool success = true;
    if (strstr(argv, "-user") == argv) // Confirm that the string starts with "-user" as strstr return the begining of the string if found
    {
        if (strlen(gUserName) == 0) // Confirming that this is the first time gUserName is being set
        {
            // Copy the argument into gUserName, but only NAME_MAX_CHARACTERS number of characters
            strncpy(gUserName, argv + strlen("-user"), NAME_MAX_CHARACTERS);
        }
        else 
        {
            // If it already has been set, 2 '-user' flags were used
            success = false;
        }
    }
    else if (strstr(argv, "-server") == argv) // Confirm that the string starts with "-user" as strstr return the begining of the string if found
    {
        if (strlen(gServerName) == 0) // Confirming that this is the first time gServerName is being set
        {
            // Copy the argument into gServerName
            strcpy(gServerName, argv + strlen("-server"));
        }
        else
        {
            // If it already has been set, 2 '-server' flags were used
            success = false;
        }
    }
    else
    {
        // The argument was not recognised and thus there was an error in the input
        success = false;
    }

    return success;
}

WINDOW* newWindow(int height, int width, int y, int x, int colorNumber)
{
    // FUNCTION     : newWindow
    // DESCRIPTION  : This function spawns the getUserMessage and getServerMessage threads
    // PARAMETERS   :   
    //    int height        : The height the window should be initialized to
    //    int width         : The height the window should be initialized to
    //    int x             : The x-coordinate of the top left corner the window should be initialized to
    //    int y             : The y-coordinate of the top left corner the window should be initialized to
    //    int colorNumber   : The color pair number to initalize the window with
    // RETURNS      : 
    //    WINDOW*           : The final initialized window

    WINDOW* win;
    win = newwin(height, width, y, x);
    box(win,0,0);
    wmove(win,1,1);
    keypad(win, TRUE);
    scrollok(win, TRUE);
    wbkgd(win, COLOR_PAIR(colorNumber));
    wrefresh(win);    
    return win;
}

void clearWindow(WINDOW* win)
{
    // FUNCTION     : clearWindow
    // DESCRIPTION  : This function clears the given window
    // PARAMETERS   :   
    //    WINDOW*   win     : The window to be cleared
    // RETURNS      : 
    //    VOID

	int x, y;
	getmaxyx(win, y, x);
	for (int i = 0;  i < y; i++)
	{
		wmove(win, i, 0);
		wclrtoeol(win);
	}
	box(win, 0, 0);
}

void printIntro()
{
    // FUNCTION     : printIntro
    // DESCRIPTION  : This function prints the intro lines to the display
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    addMessageToDisplay(" ######  ########             ###    ##       ");
    addMessageToDisplay("##    ## ##     ##           ## ##   ##    ## ");
    addMessageToDisplay("##       ##     ##          ##   ##  ##    ## ");
    addMessageToDisplay(" ######  ########  ####### ##     ## ##    ## ");
    addMessageToDisplay("      ## ##                ######### #########");
    addMessageToDisplay("##    ## ##                ##     ##       ## ");
    addMessageToDisplay(" ######  ##                ##     ##       ## ");
    addMessageToDisplay("==============================================");
    addMessageToDisplay("           Alex Kozak & Attila Katona         ");
    addMessageToDisplay("                                              ");
}

void intializeColors()
{
    // FUNCTION     : intializeColors
    // DESCRIPTION  : This function initializes the colors and the color pairs
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    start_color();
    init_pair(WHITE_BACKGROUND, COLOR_BLACK, COLOR_WHITE);
    init_pair(BLACK_BACKGROUND, COLOR_WHITE, COLOR_BLACK);   
    use_default_colors(); 
}

void initalizeWindows()
{
    // FUNCTION     : initalizeWindows
    // DESCRIPTION  : This function initializes the global windows
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    incomingTitleWindow     = newWindow(TITLES_HEIGHT, WINDOW_WIDTH, TOP_LEFT, TOP_LEFT, WHITE_BACKGROUND);
    incomingMessageWindow   = newWindow(INCOMING_HEIGHT, WINDOW_WIDTH, TITLES_HEIGHT, TOP_LEFT, BLACK_BACKGROUND);

    outgoingTitleWindow     = newWindow(TITLES_HEIGHT, WINDOW_WIDTH, OUTGOING_START_Y - TITLES_HEIGHT, WINDOW_START_X, WHITE_BACKGROUND);
    outgoingMessageWindow   = newWindow(OUTGOING_HEIGHT, WINDOW_WIDTH, OUTGOING_START_Y, WINDOW_START_X, BLACK_BACKGROUND);
}

int initalizeAndConnectToSocket()
{
    // FUNCTION     : initalizeAndConnectToSocket
    // DESCRIPTION  : This function initializes the socket connection with the 
    //                server provided by the user
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    int       : Returns -1 if the connection failed, another number of successful

    // Make socket
    gSocketID = socket(AF_INET, SOCK_STREAM, 0);
    // attr
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(gServerName); //127.0.0.1 for local connections
    address.sin_port = htons(SOCKET_NUMBER);
    return connect(gSocketID, (struct sockaddr *)&address, sizeof(address));
}

void spawnThreads()
{
    // FUNCTION     : spawnThreads
    // DESCRIPTION  : This function spawns the getUserMessage and getServerMessage threads
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    write(gSocketID,gUserName,strlen(gUserName));
    pthread_t threads[2];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    // Spawn the listen/receive deamons
    pthread_create(&threads[0], &attr, getUserMessage, NULL);
    pthread_create(&threads[1], &attr, getServerMessage, NULL);
}

void pauseAndCleanWindows()
{
    // FUNCTION     : pauseAndCleanWindows
    // DESCRIPTION  : This function waits for the user to enter a character then deletes all of the ncurses windows
    // PARAMETERS   :   
    //    VOID
    // RETURNS      : 
    //    VOID

    wgetch(outgoingMessageWindow);
    delwin(outgoingMessageWindow);
    delwin(outgoingTitleWindow);
    delwin(incomingMessageWindow);
    delwin(incomingTitleWindow);
    endwin();
}

int main(int argc, char *argv[])
{
    if ((argc == 3 && (!parseArgument(argv[1]) || !parseArgument(argv[2]))) || argc != 3)
    {
        // If there was a problem parsing the command line arguments, print this to the console and exit
        printf("Proper Usage: chat-client -user<userID> -server<server name>\n");
    }
    else
    {
        // Initialize ncurses functionality
        initscr();   
        intializeColors();
        initalizeWindows();

        // print the inital items into the headers and the intro screen
        printHeader(OUT_TITLE_TEXT);
        printIntro();        

        // Attempt to connect to the server
        if(initalizeAndConnectToSocket() == -1)
        {
            // if it failed to connect, print this message in the header
            printHeader("Connection failed, try again.");
        }
        else
        {
            spawnThreads();

            while(!done);
            addMessageToDisplay("==============================================");
            addMessageToDisplay("Press any key to exit...");
        }
        // wait for the user to enter a value and then exit
        pauseAndCleanWindows();
    }   
    return 0;
}