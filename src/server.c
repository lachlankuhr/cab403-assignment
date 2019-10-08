#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
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
#define MAXDATASIZE 30000
#define NUMCHANNELS 255

// Global variables
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

    // Initalise message lists
    msgnode_t* messages[NUMCHANNELS];
    for (int i = 0; i < NUMCHANNELS; i++) {
        messages[i] = NULL;
    }

    // Initalise array containing counts of all the messages sent to channels
    int messages_counts[NUMCHANNELS];
    for (int i = 0; i < NUMCHANNELS; i++) {
        messages_counts[i] = 0;
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
        
        /*
        pid_t pid;
        pid = fork();
        if (pid == 0) { // Child carries on processing
            for(;;) {

            }
        } else { // parent loops back around 
            for (;;) {

            }
        }*/

        // Set up new client
        client_t new_client;
        new_client.id = client_id; // set client id
        new_client.socket = new_fd; // set socket
        for (int i = 0; i < NUMCHANNELS; i++) { // set subscribed channels
            new_client.channels[i].subscribed = 0;
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
        int channel_id;
        // Wait for incoming commands (remember we only need to have one client connect right)
        while (1) {
            // Reset the variables
            command_name = "";
            char * message = (char*)malloc(MAXDATASIZE);
            channel_id = -1;

            // Receive command
            if ((numbytes = recv(client->socket, buf, MAXDATASIZE, 0)) == -1) {
                    perror("recv");
            }
            if (numbytes == 0) {
                printf("Disconnect detected.\n");
                break;
            }
            char* command = buf;

            decode_command(client, command, command_name, &channel_id, message);

            // Handle commands
            if (strcmp(command, "SUB") == 0 && channel_id != 65535) { // Cant be -1 because of uint16_t
                subscribe(channel_id, client, messages);

            } else if (strcmp(command, "CHANNELS") == 0) {
                channels (client, messages, messages_counts);

            } else if (strcmp(command, "UNSUB") == 0 && channel_id != -1) {
                unsubscribe(channel_id, client);

            } else if (strcmp(command, "NEXT") == 0 && channel_id != -1) {
                nextChannel(channel_id, client, messages);
                //printf("NEXT command with channel ID %d entered.\n", channel_id);

            } else if (strcmp(command, "NEXT") == 0 && channel_id == -1) {
                next(client, messages);

            } else if (strcmp(command, "SEND") == 0 && channel_id != -1) {
                // Increase 
                messages_counts[channel_id] += 1;

                // This is done this way due to pointer scope :) in a function means stack variables are destroyed
                msg_t* msg_struct = (msg_t *)malloc(sizeof(msg_t)); // Construct the message object
                msg_struct->string = message;
                msg_struct->user = client->id;
                msg_struct->time = time(NULL);
                msgnode_t *newhead = sendMsg(channel_id, msg_struct, client, messages);
                if (newhead == NULL) {
                    printf("Memory allocation error");
                    return EXIT_FAILURE;
                }
                messages[channel_id] = newhead;
                printf("Sent to %d: %s\n", channel_id, messages[channel_id]->msg->string);
                // Send back nothing because the 
                if (send(client->socket, "", MAXDATASIZE, 0) == -1) {
                    perror("send");
                }

            } else if (strcmp(command, "BYE") == 0 && channel_id == -1) {
                printf("BYE command entered.\n");

            } else if (strcmp(command, "STOP") == 0) {
                // Terminate all outstanding NEXT and LIVEFEED commands

            } else {
                if (send(client->socket, "Invalid command\n", MAXDATASIZE, 0) == -1) {
                    perror("send");
                }
            }
        }
    }
    
    return 0;
}

void decode_command(client_t * client, char * command, char * command_name, int * channel_id, char * message) {
    if (strtok(command, "\n") == NULL) { // No command
        if (send(client->socket, "No command entered\n", MAXDATASIZE, 0) == -1) {
            perror("send");
        }
        return;
    }

    char* sep = strtok(command, " ");   // Separate command from arguments
    if (sep != NULL) command_name = sep;

    sep = strtok(NULL, " ");
    if (sep != NULL) {  // Get channel ID
        // Reset errno to 0 before call 
        errno = 0;

        // Call to strtol assigning return to number 
        char *endptr = NULL;
        *channel_id = strtol(sep, &endptr, 10);
        
        if (sep == endptr || errno == EINVAL || (errno != 0 && *channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
            if (send(client->socket, "Invalid channel ID\n", MAXDATASIZE, 0) == -1) {
                perror("send");
            }
            *channel_id = -1;
        }
        sep = strtok(NULL, ""); // this SHOULD be "" instead of " ". It gets the rest of the message.
    } 
    if (sep != NULL) { // Get msg (for send only)
        strcpy(message, sep);
    } else {
        strcpy(message, "");
    }
}

void subscribe(int channel_id, client_t *client, msgnode_t** messages) {
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d.\n", channel_id); // will actually need to send back to client but this will do for now
    } else if (client->channels[channel_id].subscribed == 1) {
        sprintf(return_msg, "Already subscribed to channel %d.\n", channel_id);
    } else {
        sprintf(return_msg, "Subscribed to channel %d.\n", channel_id);
        client->channels[channel_id].subscribed = 1;
        client->read_msg[channel_id] = messages[channel_id]; // last message in the channel - want to track read messages
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void channels(client_t *client, msgnode_t** msg_list, int * messages_counts) {
    char return_msg[MAXDATASIZE];
    for (int channel_id = 0; channel_id < NUMCHANNELS; channel_id++) {
        if (client->channels[channel_id].subscribed == 1) {
            sprintf(buf, "%d\t%d\t%d\t%d\n", channel_id, messages_counts[channel_id], client->channels[channel_id].read, get_number_unread_messages(channel_id, client, msg_list));
            strcat(return_msg, buf);
        }
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void unsubscribe(int channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id); // will actually need to send back to client but this will do for now
    } else if (client->channels[channel_id].subscribed == 0) {
        sprintf(return_msg, "Not subscribed to channel %d\n", channel_id);
    } else {
        sprintf(return_msg, "Unsubscribed from channel %d\n", channel_id);
        client->channels[channel_id].subscribed = 0;
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void next(client_t *client, msgnode_t** msg_list) {
    char return_msg[MAXDATASIZE];
    time_t min_time = time(NULL);
    int next_channel;
    msg_t * next_msg = NULL;
    int client_subscribed_to_any_channel = 0;
    for (int channel_id = 0; channel_id <  NUMCHANNELS; channel_id++) {
        if (client->channels[channel_id].subscribed == 1) {
            client_subscribed_to_any_channel = 1;
            msg_t * message = get_next_message(channel_id, client, msg_list); // just get and not move head forward
            if (message != NULL) { // could use short circuiting
                if (message->time < min_time) {
                    min_time = message->time;
                    next_msg = message;
                    next_channel = channel_id;
                }
            }
        }
    }
    if (client_subscribed_to_any_channel == 0) {
        sprintf(return_msg, "Not subscribed to any channels.\n");
    } else if (next_msg == NULL) {
        return_msg[0] = 0; // empty string
    } else {
        read_message(next_channel, client, msg_list); // move head foward
        sprintf(return_msg, "%d: %s\n", next_channel, next_msg->string); // TODO: insert channel ID prefix as in SPEC
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}

void nextChannel(int channel_id, client_t* client, msgnode_t** msg_list) {
    char return_msg[MAXDATASIZE];
    msg_t* message_to_read; 
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id); // will actually need to send back to client but this will do for now
    } else if (client->channels[channel_id].subscribed == 0) {
        sprintf(return_msg, "Not subscribed to channel %d\n", channel_id);
    } else {
        message_to_read = read_message(channel_id, client, msg_list);
        if (message_to_read == NULL) {
            return_msg[0] = 0; // empty string
        } else {
            sprintf(return_msg, "%d:%s\n", channel_id, message_to_read->string); 
        }
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    } 
}

void livefeed(client_t *client) {

}

void livefeedChannel(int channel_id, client_t *client) {
    
}

msgnode_t* sendMsg(int channel_id, msg_t *msg, client_t *client, msgnode_t** msg_list) {
    // Check for invalid channel
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id);
        if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
            perror("send");
        }
    }

    // Construct the node for the linked list
    msgnode_t *newhead = node_add(msg_list[channel_id], msg);

    return newhead; // return
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

int setServerPort(int argc, char ** argv) {
    int port_number = 12345;
    if (argc > 1) {
        int port_number = atoi(argv[1]);
    }
    return port_number;
}   

void startServer(int argc, char ** argv) {
    // Set the server port
    int port_number = setServerPort(argc, argv);

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
    
	printf("Server is listening on port: %i.\n", port_number);
}

msgnode_t * node_add(msgnode_t *head, msg_t *message) {
    // create new node to add to list
    msgnode_t *new = (msgnode_t *)malloc(sizeof(msgnode_t));
    if (new == NULL) {
        return NULL;
    }

    // insert new node
    new->msg = message;
    new->next = head;
    return new;
}

msg_t* read_message(int channel_id, client_t* client, msgnode_t** msg_list) {
    msgnode_t* last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t* curr_head = msg_list[channel_id]; // current head, need to keep moving it back
    if (curr_head == last_read) {
        return NULL;
    }
    while (curr_head->next != last_read) {
        curr_head = curr_head->next;
    }
    client->read_msg[channel_id] = curr_head;

    // Increase the number of read messages
    client->channels[channel_id].read += 1;

    return(curr_head->msg);
}

msg_t* get_next_message(int channel_id, client_t* client, msgnode_t** msg_list) {
    msgnode_t* last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t* curr_head = msg_list[channel_id]; // current head, need to keep moving it back
    if (curr_head == last_read) {
        return NULL;
    }
    while (curr_head->next != last_read) {
        curr_head = curr_head->next;
    }
    return(curr_head->msg);
}

int get_number_unread_messages(int channel_id, client_t* client, msgnode_t** msg_list) {
    msgnode_t* last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t* curr_head = msg_list[channel_id]; // current head, need to keep moving it back
    int number_unread = 0;
    if (curr_head == last_read) {
        return number_unread;
    }
    while (curr_head->next != last_read) {
        curr_head = curr_head->next;
        number_unread += 1;
    }
    return number_unread + 1; // + 1 is required
}