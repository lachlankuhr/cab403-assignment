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
#define MAXCLIENTS 10

// Global variables
int sockfd, new_fd;                // Listen on sock_fd, new connection on new_fd
struct sockaddr_in server_addr;    // Server address information
struct sockaddr_in client_addr;    // Client address information
socklen_t sin_size;
msgnode_t* messages[NUMCHANNELS];
int messages_counts[NUMCHANNELS];
int messages_lock;
int exiting = 0;

// Receving data
int numbytes;                      // Number of bytes received from clien
char buf[MAXDATASIZE];             // Buffer to write to


int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Start the server
    startServer(argc, argv);
    int client_id = 0;

    pid_t pids[MAXCLIENTS];
    int num_clients = 0;

    while (1) {
        // Keeep checking for new connections
        sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			perror("accept");
		}
		printf("Server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
        client_id++;
        num_clients++;

        pids[num_clients-1] = fork();
        if (pids[num_clients-1] > 0) { // Parent looks for new connections
            // Parent able to do nothingish and restart the while loop
            // Currently multiple clients can connect but they act fully independently

            // TODO: ALLOW MULTIPLE CLIENTS

            // Shared memory / message board (Lec and prac 3)
                // Cannot duplicate messages (think already dealt with)
                // Use dynamic memory for messages (think already done)
            // Critical selection problem  (lec 5 reuse reader writer solution)
                // Sending message is a writer, receiving is a reader
            // Use mutexes, avoid global locks

        } else if (pids[num_clients-1] == 0) { // Child contiinues processing while loop 
            client_t* client = client_setup(client_id);
            client_processing(client); // Loops

            // TODO: Tell parent of termination and/or figure out logic to update
            // the pids array that doesn't leave bubbles when the 'not-last' process stops

        } else {
            // Fork failed
            printf("fork() failed\n");
            return -1; // TODO: Consistent failure error handling everywhere
        }
    }
    return 0;
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

    // Initalise message lists
    for (int i = 0; i < NUMCHANNELS; i++) {
        messages[i] = NULL;
    }

    // Initalise array containing counts of all the messages sent to channels
    for (int i = 0; i < NUMCHANNELS; i++) {
        messages_counts[i] = 0;
    }
}


int setServerPort(int argc, char ** argv) {
    int port_number = 12345;
    if (argc > 1) {
        port_number = atoi(argv[1]);
    }
    return port_number;
}


client_t* client_setup(int client_id) {
    //Setup new client
    client_t new_client;
    new_client.id = client_id; // set client id
    new_client.socket = new_fd; // set socket
    for (int i = 0; i < NUMCHANNELS; i++) { // set subscribed channels
        new_client.channels[i].subscribed = 0;
    }
    // Point client to new client
    client_t* client = &new_client;

    // Respond to client with welcome and choose client ID
    char msg[1024] = "Welcome! Your client ID is ";
    char id[5];

    sprintf(id, "%d.\n", client_id);
    strcat(msg, id);

    if (send(client->socket, msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
    return client;
}


void client_processing (client_t* client) {
    // What each process loops around while client active
    int channel_id;
    int run = 1;

    while (run) {
        // Wait for incoming commands
        // Reset the variables
        char* message = (char*)malloc(MAXDATASIZE);
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
        int true_neg_one = 0;

        decode_command(client, command, &channel_id, message, &true_neg_one);

        // Handle commands
        if (strcmp(command, "SUB") == 0) {
            subscribe(channel_id, client, messages);

        } else if (strcmp(command, "CHANNELS") == 0) {
            channels(client, messages, messages_counts);

        } else if (strcmp(command, "UNSUB") == 0 && (channel_id != -1 || true_neg_one == 1)) {
            unsubscribe(channel_id, client);

        } else if (strcmp(command, "NEXT") == 0) {
            if (channel_id == -1 && true_neg_one == 0) {
                next(client, messages);
            } else {
                nextChannel(channel_id, client, messages);
                // TODO: No response to client when an invalid channel is used because -1 defaults to the other next command
            }

        } else if (strcmp(command, "SEND") == 0) {
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
                exit(EXIT_FAILURE);
            }
            messages[channel_id] = newhead;
            printf("Sent to %d: %s\n", channel_id, messages[channel_id]->msg->string);
            // TODO: Fix "SEND -1 msg" printing this statement to the server (or any invalid channel)
            
            // Send back nothing because otherwise its blocking
            if (send(client->socket, "", MAXDATASIZE, 0) == -1) {
                perror("send");
            }

        } else if (strcmp(command, "BYE") == 0 && channel_id == -1) {
            run = bye(client);
            //TODO: Chop the process

        } else {
            if (strtok(command, "\n") != NULL) { // Important to prevent the no command entered and invalid command printing
                if (send(client->socket, "Invalid command\n", MAXDATASIZE, 0) == -1) {
                    perror("send");
                }
            }
        }
    }
}


void decode_command(client_t *client, char *command, int *channel_id, char *message, int *true_neg_one) {
    char original[MAXDATASIZE];
    strcpy(original, command);

    if (strtok(command, "\n") == NULL) { // No command
        if (send(client->socket, "No command entered\n", MAXDATASIZE, 0) == -1) {
            perror("send");
        }
        return;
    }

    char* sep = strtok(command, " ");   // Separate command from arguments
    sep = strtok(NULL, " ");

    // This weird check is needed and I don't know why
    // SUB -1 works, NEXT -2 works, but NEXT -1 errors out and sep becomes NULL
    // true_neg_one is set in the larger if statement normally
    if (sep == NULL && strcmp(original, "NEXT -1") == 0) *true_neg_one = 1;

    if (sep != NULL) {  // Get channel ID
        // Reset errno to 0 before call 
        errno = 0;

        // Call to strtol assigning return to number
        char *endptr = NULL;
        *channel_id = strtol(sep, &endptr, 10);
        
        if (*channel_id == -1) *true_neg_one = 1;

        if (sep == endptr || errno == EINVAL || (errno != 0 && *channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
            *channel_id = -1;
        }
        sep = strtok(NULL, ""); // this SHOULD be "" instead of " ". It gets the rest of the message.
    } 
    if (sep != NULL) { // Get msg (for send only)
        strcpy(message, sep);
    } else {
        strcpy(message, "");
    }
    printf("ID: %d\n", *channel_id);
    printf("Neg: %d\n", *true_neg_one);
}


void subscribe(int channel_id, client_t *client, msgnode_t** messages) {
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d.\n", channel_id); // TODO: Consider if showing -1 is good enough when a string is entered?
    
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
    strcpy(return_msg, "");

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
        sprintf(return_msg, "Invalid channel: %d\n", channel_id);
    
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
        sprintf(return_msg, "%d:%s\n", next_channel, next_msg->string); // TODO: insert channel ID prefix as in SPEC
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void nextChannel(int channel_id, client_t* client, msgnode_t** msg_list) {
    char return_msg[MAXDATASIZE];
    msg_t* message_to_read;
    
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id);
    
    } else if (client->channels[channel_id].subscribed == 0) {
        sprintf(return_msg, "Not subscribed to channel %d\n", channel_id);
    
    } else {
        message_to_read = read_message(channel_id, client, msg_list);
        if (message_to_read == NULL) {
            return_msg[0] = 0;
        } else {
            sprintf(return_msg, "%d:%s\n", channel_id, message_to_read->string); 
        }
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    } 
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


int bye(client_t *client) {
    printf("Client disconnected.\n");
    close(client->socket); // Close socket on server side
    return 0; // Required to stop server signal
}


void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining

    // Kill extra process first
    // Close everything in pids[] array


    // Dynamically allocated memory
    // Free messages
    for (int i = 0; i < NUMCHANNELS; i++) {
        msgnode_t* head = messages[i];
        while (head != NULL) {
            msgnode_t* next = head->next;
            free(head->msg->string); // free message text
            free(head->msg); // free message struct
            free(head); // free message node
            head = next;
        }
    }
    // Clients should be freed automatically because they're stack variables

    printf("\nHandling the signal gracefully...\n"); 
    // TODO: Deal with every process triggering...

    // Close sockets
    close(sockfd);
    close(new_fd);
    exit(1);
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