#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <signal.h>
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include "client.h"
#include "data.h"

#define MAXDATASIZE 1024
#define NUMCHANNELS 255
#define COMMANDSIZE 50  // This will need to be considered
#define MAX_INPUT 3

// Global variables
int sockfd;
struct hostent *he;
struct sockaddr_in server_addr;
// Receiving data
int numbytes;  
char buf[MAXDATASIZE];

int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Start the client
    startClient(argc, argv);

    // Abitarily using sizes
    char command[COMMANDSIZE];
    int subscribed_channels[NUMCHANNELS]; // array for subscribed channels - store client side and client can make requests to server
    for (int i = 0; i < NUMCHANNELS; i++) {
        subscribed_channels[i] = 0; // initialise subscribed channels array to 0
    }
    char * input[MAX_INPUT];

    // Continue to look for new comamnds
    pid_t pid;
    pid = fork();
    if (pid == 0) { // child sends messages
        for(;;) {
            // Wait for and read user input
            fgets(command, COMMANDSIZE, stdin);
            // Send command name
            if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
                perror("send");
            }
        }
    } else { // parent receives messages 
        for (;;) {
            if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv.");
            }

            buf[numbytes] = '\0';

            printf("%s", buf);
        }
    }

    return 0;
}

void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    printf("Handling the signal gracefully for thread %d...\n", 1); //TODO

    // Allowing port and address reuse is dealt with in setup

    // TODO: Handle shutdown gracefully
    // Inform clients etc

    close(sockfd);

    exit(1);
}

int setClientPort(int argc, char ** argv) {
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        exit(-1);
    } else {
        return atoi(argv[2]); // Port number
    }
}

void startClient(int argc, char ** argv) {
    // Set the client's port number
    int port_number = setClientPort(argc, argv);

    if ((he=gethostbyname(argv[1])) == NULL) {  // Get the server info
		herror("gethostbyname");
		exit(1);
	}

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	server_addr.sin_family = AF_INET;             // Host byte order */
	server_addr.sin_port = htons(port_number);    // Short, network byte order */
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8);            // Zero the rest of the struct */

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

    // Receive the startup message
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("%s", buf);

}