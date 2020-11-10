#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <iostream>
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <cstdlib>

#define WIDTH 43
#define HEIGHT 21
#define PADLX 1
#define PADRX WIDTH - 2

// Global variables recording the state of the game
// Position of ball
int ballX, ballY;
// Movement of ball
int dx, dy;
// Position of paddles
int padLY, padRY;
// Player scores
int scoreL, scoreR;

struct opponentInfo {
	int connFD;
	struct sockaddr_in * sin;
	bool host;
	char * port;
};

// ncurses window
WINDOW *win;

/* Draw the current game state to the screen
 * ballX: X position of the ball
 * ballY: Y position of the ball
 * padLY: Y position of the left paddle
 * padRY: Y position of the right paddle
 * scoreL: Score of the left player
 * scoreR: Score of the right player
 */
void draw(int ballX, int ballY, int padLY, int padRY, int scoreL, int scoreR) {
    // Center line
    int y;
    for(y = 1; y < HEIGHT-1; y++) {
        mvwaddch(win, y, WIDTH / 2, ACS_VLINE);
    }
    // Score
    mvwprintw(win, 1, WIDTH / 2 - 3, "%2d", scoreL);
    mvwprintw(win, 1, WIDTH / 2 + 2, "%d", scoreR);
    // Ball
    mvwaddch(win, ballY, ballX, ACS_BLOCK);
    // Left paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padLY - 2 && y <= padLY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADLX, ch);
    }
    // Right paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padRY - 2 && y <= padRY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADRX, ch);
    }
    // Print the virtual window (win) to the screen
    wrefresh(win);
    // Finally erase ball for next time (allows ball to move before next refresh)
    mvwaddch(win, ballY, ballX, ' ');
}

/* Return ball and paddles to starting positions
 * Horizontal direction of the ball is randomized
 */
void reset() {
    ballX = WIDTH / 2;
    padLY = padRY = ballY = HEIGHT / 2;
    // dx is randomly either -1 or 1
    dx = (rand() % 2) * 2 - 1;
    dy = 0;
    // Draw to reset everything visually
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Display a message with a 3 second countdown
 * This method blocks for the duration of the countdown
 * message: The text to display during the countdown
 */
void countdown(const char *message) {
    int h = 4;
    int w = strlen(message) + 4;
    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    box(popup, 0, 0);
    mvwprintw(popup, 1, 2, message);
    int countdown;
    for(countdown = 3; countdown > 0; countdown--) {
        mvwprintw(popup, 2, w / 2, "%d", countdown);
        wrefresh(popup);
        sleep(1);
    }
    wclear(popup);
    wrefresh(popup);
    delwin(popup);
    padLY = padRY = HEIGHT / 2; // Wipe out any input that accumulated during the delay
}

/* Perform periodic game functions:
 * 1. Move the ball
 * 2. Detect collisions
 * 3. Detect scored points and react accordingly
 * 4. Draw updated game state to the screen
 */
void tock() {
    // Move the ball
    ballX += dx;
    ballY += dy;
    
    // Check for paddle collisions
    // padY is y value of closest paddle to ball
    int padY = (ballX < WIDTH / 2) ? padLY : padRY;
    // colX is x value of ball for a paddle collision
    int colX = (ballX < WIDTH / 2) ? PADLX + 1 : PADRX - 1;
    if(ballX == colX && abs(ballY - padY) <= 2) {
        // Collision detected!
    int refresh;
        dx *= -1;
        // Determine bounce angle
        if(ballY < padY) dy = -1;
        else if(ballY > padY) dy = 1;
        else dy = 0;
    }

    // Check for top/bottom boundary collisions
    if(ballY == 1) dy = 1;
    else if(ballY == HEIGHT - 2) dy = -1;
    
    // Score points
    if(ballX == 0) {
        scoreR = (scoreR + 1) % 100;
        reset();
        countdown("SCORE -->");
    } else if(ballX == WIDTH - 1) {
        scoreL = (scoreL + 1) % 100;
        reset();
        countdown("<-- SCORE");
    }
    // Finally, redraw the current state
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Listen to keyboard input
 * Updates global pad positions
 */
void *listenInput(void *args) {
    while(1) {
        switch(getch()) {
            case KEY_UP: padRY--;
             break;
            case KEY_DOWN: padRY++;
             break;
            case 'w': padLY--;
             break;
            case 's': padLY++;
             break;
            default: break;
       }
    }       
    return NULL;
}

void initNcurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    refresh();
    win = newwin(HEIGHT, WIDTH, (LINES - HEIGHT) / 2, (COLS - WIDTH) / 2);
    box(win, 0, 0);
    mvwaddch(win, 0, WIDTH / 2, ACS_TTEE);
    mvwaddch(win, HEIGHT-1, WIDTH / 2, ACS_BTEE);
}

void * getMessage(struct opponentInfo * oppInfo) {
	char buffer[BUFSIZ];
	int addrLen = sizeof(struct sockaddr_in);
	int byt_rec;
	
	if (  (byt_rec = recvfrom(oppInfo->connFD, buffer, sizeof(buffer), 0, (struct sockaddr *) oppInfo->sin, (socklen_t *)&addrLen)) == -1 ) {
		std::cerr << "Error on recvfrom() " << strerror(errno) << std::endl;
		std::exit(1); }

	buffer[byt_rec] = '\0';
	char * toRet = buffer;
	return toRet;
}

void sendMessage(struct opponentInfo oppInfo, void * toSend, int msgSiz) {

	if (oppInfo.host) { std::cerr << "made it here\n"; }

	oppInfo.sin->sin_family = AF_INET ;
	oppInfo.sin->sin_port = htons(atoi(oppInfo.port)) ;


	int retKey;
	if	( (retKey = sendto(oppInfo.connFD,toSend, msgSiz, 0, (struct sockaddr *)oppInfo.sin, sizeof(struct sockaddr_in)) ) < 0 ) {
		std::cerr << "Error on sendto(): " << strerror(errno) << std::endl;
		std::exit(1);
	}
}

int getSock(char * port) {
	int sockfd;
	struct sockaddr_in sin;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET ;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo * results;
	int status;
	if( (status = getaddrinfo(NULL, port, &hints, &results)) != 0 ) {
		std::cerr << "Failure on getaddrinfo(): " << gai_strerror(status) << std::endl;
		std::exit(1);
	}
	if ( (sockfd = socket(results->ai_family, results->ai_socktype, 0)) < 0) {
		std::cerr << "Error on socket(): " << strerror(errno) << std::endl;
	}
	int bindRes;
	if ( (bindRes = bind(sockfd, results->ai_addr, results->ai_addrlen) ) == -1 ) {
		std::cerr << "Error on bind(): " << strerror(errno) << std::endl;
		close(sockfd);
		std::exit(1);
	}

	freeaddrinfo(results);
	return sockfd;
}

void connectToClient(struct opponentInfo * oppInfoPtr){

	//memset(oppInfoPtr->sin, 0, sizeof(struct sockaddr_in)) ;
	char * buf = (char *) getMessage(oppInfoPtr);
	std::cerr << "First Message: " << buf << std::endl;
	std::cerr << "Port: " << oppInfoPtr->sin << std::endl;

}

void connectToHost(struct opponentInfo * oppInfoPtr, char * hostName) {

	int sockfd;
	if ( (sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ) {
		std::cerr << "Error on Socket(): " << strerror(errno) << std::endl;
	}

	oppInfoPtr->connFD = sockfd;

	struct addrinfo * dest;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = 0;

	int status;
	if ( (status = getaddrinfo(hostName, oppInfoPtr->port, &hints, &dest)) != 0) {
		std::cerr << "Failure on getaddrinfo(): " << gai_strerror(status) << std::endl;
		std::exit(1);
	}

	struct sockaddr * hostAdr = dest->ai_addr ;
	oppInfoPtr-> sin = (struct sockaddr_in * ) hostAdr ;

	char msg[9] = "hey" ;

	sendMessage(*oppInfoPtr, (void *) msg, strlen(msg)+1) ;	
}

int main(int argc, char *argv[]) {

	char * hostPort;
	char * hostName;
	struct opponentInfo oppInfo;
	int refresh;
	int maxRounds;

	hostPort = argv[2];
	if ( !strcmp(argv[1], "--host") ){
		freopen("servLog.txt", "w", stderr);
		std::cerr << "HOST!" << std::endl;
		oppInfo.connFD = getSock(hostPort);
		
		// Establish Difficulty
		char difficulty[10]; 
		printf("Please select the difficulty level (easy, medium or hard): ");
		scanf("%s", &difficulty);
		if(strcmp(difficulty, "easy") == 0) refresh = 80000;
		else if(strcmp(difficulty, "medium") == 0) refresh = 40000;
		else if(strcmp(difficulty, "hard") == 0) refresh = 20000;

		// Get Max Rounds
		std::cout << "Enter maximum rounds to play: " ;
		scanf("%d", &maxRounds);

		std::cout << "Waiting For Connections on Port " << hostPort << std::endl;

		// Connect to an opponent
		connectToClient(&oppInfo);
		oppInfo.host = true;
	}else{
		freopen("cliLog.txt", "w", stderr);
		std::cerr << "GUEST!" << std::endl;
		oppInfo.host = false;
		oppInfo.port = hostPort;
		hostName = argv[1];
		connectToHost(&oppInfo, hostName);
	}

	std::cerr << "My Host Value is: " << oppInfo.host << std::endl;

	if (oppInfo.host == false) {
		std::cerr << "Ready to Rec...\n";
		char * rec = (char *) getMessage(&oppInfo) ;
		std::cerr << "Received From Host: " << rec << std::endl;
	}
	else {
		std::cerr << "About to send...\n";
		char temp [6] = "aaa";
		sendMessage(oppInfo, (void *) temp, strlen(temp)+1) ;
		std::cerr << "Sent " << temp << " to host!\n";
	}

	// --------------------//
    // Process args
    // refresh is clock rate in microseconds
    // This corresponds to the movement speed of the ball

    // Set up ncurses environment
    initNcurses();

    // Set starting game state and display a countdown
    reset();
    countdown("Starting Game");
    
    // Listen to keyboard input in a background thread
    pthread_t pth;
    pthread_create(&pth, NULL, listenInput, NULL);

    // Main game loop executes tock() method every REFRESH microseconds
    struct timeval tv;
    while(1) {
        gettimeofday(&tv,NULL);
        unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;
        tock(); // Update game state
        gettimeofday(&tv,NULL);
        unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;
        unsigned long toSleep = refresh - (after - before);
        // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()
        // In that case it's MUCH bigger because of overflow!
        if(toSleep > refresh) toSleep = refresh;
        usleep(toSleep); // Sleep exactly as much as is necessary
    }
    
    // Clean up
    pthread_join(pth, NULL);
    endwin();
    return 0;
}
