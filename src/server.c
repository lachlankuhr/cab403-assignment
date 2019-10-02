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
    while (1) {
        
        sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			perror("accept");
		}
		printf("server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
        
        // I imagine at this point we will fork, the parent fork will loop back around and
        // the child fork will carry on processing

        // Set up new client
        client_t new_client;
        new_client.id = client_id; // set client id
        new_client.socket = new_fd; // set socket
        //fcntl(new_client.socket, F_SETFL, O_NONBLOCK); // non-blocking
        for (int i = 0; i < NUMCHANNELS; i++) { // set subscribed channels
            new_client.channels[i] = 0;
        }
        
        // point client to new client
        client_t* client = &new_client;

        // Respond to client with welcome and choose client ID
        char msg[1024] = "Welcome! Your client ID is ";
        char id[5];

        sprintf(id, "%d.\n", client_id++);
        strcat(msg, id);

        if (send(client->socket, msg, MAXDATASIZE, 0) == -1) {
            perror("send");
        }

        char * command_name;
        char * message;
        int channel_id;
        // Wait for incoming commands (remember we only need to have one client connect right)
        while (1) {
            // Reset the variables
            command_name = "";
            channel_id = -1;
            message = "";

            // Receive command
            if ((numbytes = recv(client->socket, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv");
            }
            if (numbytes == 0) {
                printf("Disconnect detected.\n");
                break;
            }
            char* command = buf;

            if (strtok(command, "\n") == NULL) { // No command
                printf("No command entered\n");
                continue;
            }

            char* sep = strtok(command, " ");   // Separate command from arguments
            if (sep != NULL) command_name = sep;

            sep = strtok(NULL, " ");
            if (sep != NULL) {                           // Get channel ID
                printf("%s",sep);
                // Reset errno to 0 before call 
                errno = 0;

                // Call to strtol assigning return to number 
                char *endptr = NULL;
                channel_id = strtol(sep, &endptr, 10);
                
                if (sep == endptr || errno == EINVAL || (errno != 0 && channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
                    printf("Invalid channel ID\n");      //TODO: Proper validation
                    channel_id = -1;
                }
                sep = strtok(NULL, ""); // this SHOULD be "" instead of " ". It gets the rest of the message.
            } 
            // The message is considered to be the rest of command (including
            // spaces) after the second parameter. 
            // Checking for too many parameters should be done later and 
            // dependent on what the command entered was. 

            if (sep != NULL) {                  // Get msg (for send only)
                message = sep;                      // TODO: Msg format validation
            }

            printf("Command entered: %s\n", command_name);
            printf("Channel ID entered: %d\n", channel_id);
            printf("Message entered: %s\n", message);

            // This one will be for messages - coming back to it
            // if ((numbytes = recv(new_fd, buf, MAXDATASIZE, 0)) == -1) {
            //     perror("recv");
            //     exit(1);
            // }
            //int channel_id = 0; // here for sake of consistency

            // handle commands - I think we're better off doing it here rather than the client
            if (strcmp(command, "SUB") == 0 && channel_id != 65535) { // Cant be -1 because of uint16_t
                subscribe(channel_id, client);

            } else if (strcmp(command, "CHANNELS") == 0) {
                channels(client);

            } else if (strcmp(command, "UNSUB") == 0 && channel_id != -1) {
                unsubscribe(channel_id, client);
                //printf("UNSUB command entered with channel %d\n", channel_id);

            } else if (strcmp(command, "NEXT") == 0 && channel_id != -1) {
                printf("NEXT command with channel ID %d entered.\n", channel_id);

            } else if (strcmp(command, "NEXT") == 0 && channel_id == -1) {
                printf("NEXT command without channel ID entered.\n");

            } else if (strcmp(command, "LIVEFEED") == 0 && channel_id != -1) {
                printf("LIVEFEED command with channel ID %d entered.\n", channel_id);

            } else if (strcmp(command, "LIVEFEED") == 0 && channel_id == -1) {
                printf("LIVEFEED command without channel ID entered.\n");

            } else if (strcmp(command, "SEND") == 0) {
                printf("SEND command entered.\n");
                //printf("%s\n", msg);

            } else if (strcmp(command, "BYE") == 0 && channel_id == -1) {
                printf("BYE command entered.\n");

            } else {
                printf("Command entered is not valid.\n");
            }
        }
    }
    
    return 0;
}

void subscribe(int channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];

    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d.\n", channel_id); // will actually need to send back to client but this will do for now
    } else if (client->channels[channel_id] == 1) {
        sprintf(return_msg, "Already subscribed to channel %d.\n", channel_id);
    } else {
        sprintf(return_msg, "Subscribed to channel %d.\n", channel_id);
        client->channels[channel_id] = 1;
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void channels(client_t *client) {
    for (int i = 0; i < NUMCHANNELS; i++) {
        if (client->channels[i] == 1){
            sprintf(buf, "%d\tmsg\treadmsg\tunreadmsg\n", i);
            if (send(client->socket, buf, MAXDATASIZE, 0) == -1) {
                perror("send");
            }
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

void nextChannel(int channel_id, client_t *client) {

}

void livefeed(client_t *client) {

}

void livefeedChannel(int channel_id, client_t *client) {

}

void sendMsg(int channel_id, char* msg, client_t *client) {

}

void bye(client_t *client) {

}

void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    printf("Handling the signal gracefully...\n");

    // Allowing port and address reuse is dealt with in setup

    // TODO: Handle shutdown gracefully
    // Inform clients etc
    close(sockfd);
    close(new_fd);
    exit(1);
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
    //fcntl(sockfd, F_SETFL, O_NONBLOCK); // Socket to non-blocking state
    //fcntl(new_fd, F_SETFL, O_NONBLOCK); // Socket to non-blocking state
    

	printf("Server is listening on port: %i.\n", port_number);
}