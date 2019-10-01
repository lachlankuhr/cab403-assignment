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
//#include "data.h"

#define BACKLOG 10     // How many pending connections queue will hold
#define MAXDATASIZE 1024
#define NUMCHANNELS 255

// Variable to keep program running until SIGINT occurs
static volatile sig_atomic_t keep_running = 1;

// Global variables
int port_number; 
int sockfd, new_fd;                // Listen on sock_fd, new connection on new_fd
struct sockaddr_in server_addr;    // Server address information
struct sockaddr_in client_addr;    // Client address information
socklen_t sin_size;

// Receving data
int numbytes;                      // Number of bytes received from clien
char buf[MAXDATASIZE];             // Buffer to write to

int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Initalise channel listings - one for client connections and one for messages
    clist_t client_subs;
    msglist_t messages;
    if (!clist_init(&client_subs, NUMCHANNELS)) {
       printf("Failed to initialise channels\n");
       return EXIT_FAILURE;
    }
    if (!msglist_init(&messages, NUMCHANNELS)) {
       printf("Failed to initialise channels\n");
       return EXIT_FAILURE;
    }

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
        
        // I imagine at this point we will fork, the parent fork will loop back around and
        // the child fork will carry on processing

        // Set up new client
        client_t new_client;
        new_client.id = client_id; // set client id
        new_client.socket = new_fd; // set socket
        for (int i = 0; i < NUMCHANNELS; i++) { // set subscribed channels
            new_client.channels[i] = 0;
        }
        
        // point client to new client
        client_t* client = &new_client;

        // Respond to client with welcome and choose client ID
        char msg[1024] = "Welcome! Your client ID is ";
        char id[5];

        sprintf(id, "%d\n", client_id++);
        strcat(msg, id);

        if (send(client->socket, msg, MAXDATASIZE, 0) == -1) {
            perror("send");
        }

        // wait for incoming commands (remember we only need to have one client connect right)
        while (keep_running) {
            // Receive command
            if ((numbytes = recv(client->socket, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv");
                exit(1);
            }
            char* command = buf;

            // Receive channel ID
            uint16_t received;
            int channel_id;
            if ((numbytes = recv(client->socket, &received, sizeof(uint16_t), 0)) == -1) {
                perror("recv");
                exit(1);
            }
            channel_id = ntohs(received);

            // This one will be for messages - coming back to it
            // if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
            //     perror("recv");
            //     exit(1);
            // }
            //int channel_id = 0; // here for sake of consistency

            // handle commands - I think we're better off doing it here rather than the client
            if (strcmp(command, "SUB\n") == 0 && channel_id != 65535) { // Cant be -1 because of uint16_t
                subscribe(channel_id, client);

            } else if (strcmp(command, "CHANNELS\n") == 0) {
                channels(client);

            } else if (strcmp(command, "UNSUB\n") == 0 && channel_id != 65535) {
                unsubscribe(channel_id, client);
                //printf("UNSUB command entered with channel %d\n", channel_id);

            } else if (strcmp(command, "NEXT\n") == 0 && channel_id != 65535) {
                printf("NEXT command with channel ID %d entered.\n", channel_id);

            } else if (strcmp(command, "NEXT\n") == 0 && channel_id == 65535) {
                printf("NEXT command without channel ID entered.\n");

            } else if (strcmp(command, "LIVEFEED\n") == 0 && channel_id != 65535) {
                printf("LIVEFEED command with channel ID %d entered.\n", channel_id);

            } else if (strcmp(command, "LIVEFEED\n") == 0 && channel_id == 65535) {
                printf("LIVEFEED command without channel ID entered.\n");

            } else if (strcmp(command, "SEND\n") == 0) {
                printf("SEND command entered.\n");
                //printf("%s\n", msg);

            } else if (strcmp(command, "BYE\n") == 0 && channel_id == 65535) {
                printf("BYE command entered.\n");

            } else {
                printf("Command entered is not valid.\n");
            }
        }
    }
    
    printf("The program was successfully exited.\n");
    return 0;
}

void subscribe(int channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];

    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id); // will actually need to send back to client but this will do for now
    } else if (client->channels[channel_id] == 1) {
        sprintf(return_msg, "Already subscribed to channel %d\n", channel_id);
    } else {
        sprintf(return_msg, "Subscribed to channel %d\n", channel_id);
        client->channels[channel_id] = 1;
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void channels(client_t *client) {
    for (int i = 0; i < NUMCHANNELS; i++) {
        if (client->channels[i] == 1){
            printf("%d\tmsg\treadmsg\tunreadmsg\n", i);
        }
    }
}

void unsubscribe(int channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id); // will actually need to send back to client but this will do for now
    } else if (client->channels[channel_id] == 0) {
        sprintf(return_msg, "Not subscribed to channel %d\n", channel_id);
    } else {
        sprintf(return_msg, "Unsubscribed from channel %d\n", channel_id);
        client->channels[channel_id] = 0;
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void next(client_t *client) {

}

void next_channel(int channel_id, client_t *client) {

}

void livefeed(client_t *client) {

}

void livefeed_channel(int channel_id, client_t *client) {

}

void send_msg(int channel_id, char* msg, client_t *client) {

}

void bye(client_t *client) {

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
    //fcntl(new_fd, F_SETFL, O_NONBLOCK); // Socket to non-blocking state
    

	printf("Server is listening on port: %i.\n", port_number);
}