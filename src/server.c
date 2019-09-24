#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include <signal.h> // Signal handling
#include <arpa/inet.h>
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <unistd.h>
#include <fcntl.h> 
#include "server.h"

#define BACKLOG 10     /* how many pending connections queue will hold */

// Variable to keep program running until SIGINT occurs
static volatile sig_atomic_t keep_running = 1;

// Global variables
int port_number; 
int sockfd, new_fd;  /* listen on sock_fd, new connection on new_fd */
struct sockaddr_in server_addr;    /* server address information */
struct sockaddr_in client_addr;    /* client address information */
socklen_t sin_size;


int main(int argc, char ** argv) {
    // setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // start the server
    startServer(argc, argv);

    // Keeep checking for new connections
    while (keep_running) {
        sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			continue;
		}
		printf("server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
        if (send(new_fd, "Welcome! Your client ID is 1.\n", 35, 0) == -1) {
            perror("send");
        }
    }
    
    printf("The program was successfully exited.\n");
    return 0;
}

void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    printf("Handling the signal gracefully...\n");
    keep_running = 0;
}

void setServerPort(int argc, char ** argv) {
    if (argc > 1) {
        port_number = atoi(argv[1]);
    } else {
        port_number = 12345;
    }
    printf("The port number was set to: %i.\n", port_number);
}   

void startServer(int argc, char ** argv) {
    // Set the server port
    setServerPort(argc, argv); 
    /* generate the socket */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	/* generate the end point */
	server_addr.sin_family = AF_INET;         /* host byte order */
	server_addr.sin_port = htons(port_number);     /* short, network byte order */
	server_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
		/* bzero(&(server_addr.sin_zero), 8);   ZJL*/     /* zero the rest of the struct */

	/* bind the socket to the end point */
	if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) \
	== -1) {
		perror("bind");
		exit(1);
	}

	/* start listnening */
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

    // Set socket to be non-blocking
    fcntl(sockfd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
    fcntl(new_fd, F_SETFL, O_NONBLOCK); /* Change the socket into non-blocking state	*/
    

	printf("Server is listening on port: %i.\n", port_number);
}