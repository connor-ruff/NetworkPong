#include<iostream>
#include<cerrno>
#include<cstdlib>
#include<cstring>
#include<sys/socket.h>
#include<sys/types.h>
#include<netdb.h>
#include<netinet/in.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>

void guestMain(int sockfd, char * hostName){
}

void hostMain(int sockfd){
}

int setupHost(char * port){
	int sockfd ;
	struct sockaddr_in sin;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	struct addrinfo * results;

	/** Load host address info **/
	int status;
	if ( (status = getaddrinfo(NULL, port, &hints, &results)) != 0){
		std::cerr << "Host Failure on getaddrinfo(): " << gai_strerror(status) << std::endl;
	}

	struct addrinfo * ptr = results;

	if ( (sockfd = socket(ptr->ai_family, ptr->ai_socktype, 0)) < 0){
		std::cerr << "Host Error on Socket(): " << strerror(errno) << std::endl;
		std::exit(-1);
	}

	int bindRes;
	if (bindRes = bind(sockfd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
		std::cerr << "Host Error on Bind(): " << strerror(errno) << std::endl;
		close(sockfd);
		std::exit(-1);
	}

	freeaddrinfo(results);
	std::cout << "Ready on port " << port << " (socket #: " << sockfd << ")\n";
}

int main(int argc, char ** argv){

	if (argc != 3){
		std::cerr << "Wrong Number of Arguments\n" ;
		std::exit(-1);
	}
	char * port = argv[2];
	int sockfd;

	// Check if Host
	if (strcmp(argv[1], "--host") == 0){

		// Create Socket
		sockfd = setupHost(port);
		// Send To Main Function for Host
		hostMain(sockfd);
	}
	
	// Not Host!
	else{
		// Create Socket
		if( (sockfd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
			std::cerr << "Failure Setting Up Guest Socket on socket(): " << strerror(errno) << std::endl;
			std::exit(-1);
		}
		// Send to Main Function for Guest
		char * hostName = argv[1];
		guestMain(sockfd, hostName);
	}
	

	return 0;
}

