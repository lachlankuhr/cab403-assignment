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

#define BACKLOG 10     // How many pending connections queue will hold

// Variable to keep program running until SIGINT occurs
static volatile sig_atomic_t keep_running = 1;

// Global variables
int port_number; 
int sockfd, new_fd;                // Listen on sock_fd, new connection on new_fd
struct sockaddr_in server_addr;    // Server address information
struct sockaddr_in client_addr;    // Client address information
socklen_t sin_size;


int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Start the server
    startServer(argc, argv);

    int client_id = 1;

    // Keeep checking for new connections
    while (keep_running) {
        
        sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			continue;
		}
		printf("server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));

        // Respond to client with welcome and choose client ID
        char msg[1024] = "Welcome! Your client ID is ";
        char id[5];

        sprintf(id, "%d\n", client_id++);
        strcat(msg, id);

        if (send(new_fd, msg, 1024, 0) == -1) {
            perror("send");
        }
    }
    
    printf("The program was successfully exited.\n");
    return 0;
}

void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    printf("Handling the signal gracefully...\n");

    // Allowing port and address reuse is dealt with in setup

    // TODO: Handle shutdown gracefully
    // Inform clients etc

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

    // Generate socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(0);
	}

	// Generate the end point
	server_addr.sin_family = AF_INET;              // Host byte order
	server_addr.sin_port = htons(port_number);     // Short, network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY;      // Auto-fill with my IP
	bzero(&(server_addr.sin_zero), 8);             // Zero the rest of the struct

    // Set options allowing address and port reuse
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    #ifdef SO_REUSEPORT
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char*)&reuse, sizeof(reuse)) < 0) 
        perror("setsockopt(SO_REUSEPORT) failed");
    #endif

	// Bind the socket to the end point
	if (bind(sockfd, (struct sockaddr *)&server_addr, 
    sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(-1);
	}

	// Start listnening
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(0);
	}

    // Set socket to be non-blocking
    fcntl(sockfd, F_SETFL, O_NONBLOCK); // Socket to non-blocking state
    fcntl(new_fd, F_SETFL, O_NONBLOCK); // Socket to non-blocking state
    

	printf("Server is listening on port: %i.\n", port_number);
}