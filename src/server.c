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
#include <sys/mman.h>
#include "server.h"

#define BACKLOG 10              // How many pending connections queue will hold
#define MAXDATASIZE 30000
#define MAXMESSAGES 1000
#define MAXMESSAGELENGTH 1024
#define NUMCHANNELS 255
#define MAXCLIENTS 10


// Global variables
int sockfd, new_fd;       // Listen on sock_fd, new connection on new_fd
char buf[MAXDATASIZE];    // Craps itself if I try to make this local...
pid_t pids[MAXCLIENTS];
int num_clients = 0;

// Shared memory address shortcut pointers
msgnode_t **messages;       // Array of pointers to msg_t node heads
msgnode_t *messages_nodes;  // Array of all msg_t nodes
msg_t *messages_msg;        // Array of all msg_t messages
int *messages_counts;       // Array of msg counts by channel, last index is total
int smfd1, smfd2;           // SHM handle IDs

int main(int argc, char **argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);
    setupSharedMem();
    
    // Start the server
    startServer(argc, argv);
    struct sockaddr_in client_addr;
    socklen_t sin_size;
    int client_id = 0;

    while (1) {
        // Keeep checking for new connections
        sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size)) == -1) {
			perror("accept");
		}
		printf("Server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
        client_id++;
        num_clients++;

        pids[num_clients-1] = fork();
        if (pids[num_clients-1] > 0) {
            // Parent continues to look for new connections in above while loop section.

        } else if (pids[num_clients-1] == 0) { // Child contiinues processing while loop 
            
            client_t *client = client_setup(client_id);
            client_processing(client); // Loops

            // Critical selection problem  (lec 5 reuse reader writer solution)
            // Use mutexes, avoid global locks. Use pthread_rwlock_t

            // TODO: Tell parent of termination and/or figure out logic to update
            // the pids array that doesn't leave bubbles when the 'not-last' process stops

        } else {
            // Fork failed
            printf("fork() failed\n");
            return -1;
        }
    }
    shm_unlink("/messages");
    shm_unlink("/counts");
    return 0;
}


void setupSharedMem() {

    size_t size1 = NUMCHANNELS * sizeof(msgnode_t*);
    size_t size2 = MAXMESSAGES * sizeof(msgnode_t);
    size_t size3 = MAXMESSAGES * sizeof(msg_t);
    size_t size4 = (NUMCHANNELS+1) * sizeof(int);

    // Create a shared memory objects
    smfd1 = shm_open("/messages", O_RDWR | O_CREAT, 0600);
    smfd2 = shm_open("/counts", O_RDWR | O_CREAT, 0600);

    // Resize to fit
    ftruncate(smfd1, size1 + size2 + size3);
    ftruncate(smfd2, size4);

    // Map objects
    messages = mmap(NULL, size1, PROT_READ | PROT_WRITE, MAP_SHARED, smfd1, 0); // Must be in SHM (by experimentation)
    messages_nodes = mmap(NULL, size2, PROT_READ | PROT_WRITE, MAP_SHARED, smfd1, 0);
    messages_msg = mmap(NULL, size3, PROT_READ | PROT_WRITE, MAP_SHARED, smfd1, 0);
    messages_counts = mmap(NULL, size4, PROT_READ | PROT_WRITE, MAP_SHARED, smfd2, 0);

    // Initalise message list heads
    for (int i = 0; i < NUMCHANNELS; i++) {
        messages[i] = NULL;
    }
    // Initalise counts of messages sent to channels. Last index is total.
    for (int i = 0; i < NUMCHANNELS+1; i++) {
        messages_counts[i] = 0;
    }
}


void startServer(int argc, char **argv) {
    // Set the server port
    int port_number = setServerPort(argc, argv);
    struct sockaddr_in server_addr;
    
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
	if (bind(sockfd, (struct sockaddr*)&server_addr, 
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


int setServerPort(int argc, char **argv) {
    int port_number = 12345;                   // DON'T FORGET THIS IS HARDCODED
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
        new_client.channels[i].read = 0;
    }
    // Point client to new client
    client_t *client = &new_client;

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


void client_processing (client_t *client) {
    // What each process loops around while client active
    int channel_id;
    int run = 1;
    int numbytes;

    while (run) {
        // Wait for incoming commands
        // Reset the variables
        char message[MAXMESSAGELENGTH];
        channel_id = -1;

        // Receive command
        if ((numbytes = recv(client->socket, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv");
        }
        if (numbytes == 0) {
            printf("Disconnect detected.\n");
            break;
        }
        char *command = buf;
        int true_neg_one = 0;

        decode_command(client, command, &channel_id, message, &true_neg_one);

        // Handle commands
        if (strcmp(command, "SUB") == 0) {
            subscribe(channel_id, client);

        } else if (strcmp(command, "CHANNELS") == 0) {
            channels(client);

        } else if (strcmp(command, "UNSUB") == 0 && (channel_id != -1 || true_neg_one == 1)) {
            unsubscribe(channel_id, client);

        } else if (strcmp(command, "NEXT") == 0) {
            if (channel_id == -1 && true_neg_one == 0) {
                next(client);
            } else {
                nextChannel(channel_id, client);
                // TODO: No response to client when an invalid channel is used because -1 defaults to the other next command
            }

        } else if (strcmp(command, "SEND") == 0) {
            messages_counts[NUMCHANNELS]++; // Increments total message value stored at end of array
            // Increase
            sendMsg(channel_id, client, message);

        } else if (strcmp(command, "BYE") == 0 && channel_id == -1) {
            run = bye(client);

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

    char *sep = strtok(command, " ");   // Separate command from arguments
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
}


void subscribe(int channel_id, client_t *client) {
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


void channels(client_t *client) {
    char return_msg[MAXDATASIZE];
    strcpy(return_msg, "");
    
    for (int channel_id = 0; channel_id < NUMCHANNELS; channel_id++) {
        if (client->channels[channel_id].subscribed == 1) {
            sprintf(buf, "%d\t%ld\t%d\t%d\n", channel_id, (long)messages_counts[channel_id],
                client->channels[channel_id].read, get_number_unread_messages(channel_id, client));
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


void next(client_t *client) {
    char return_msg[MAXDATASIZE];
    time_t min_time = time(NULL);
    int next_channel;
    msg_t *next_msg = NULL;
    int client_subscribed_to_any_channel = 0;


    for (int channel_id = 0; channel_id <  NUMCHANNELS; channel_id++) {
        if (client->channels[channel_id].subscribed == 1) {
            client_subscribed_to_any_channel = 1;
            msg_t *message = get_next_message(channel_id, client); // just get and not move head forward
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
        read_message(next_channel, client); // move head foward
        sprintf(return_msg, "%d:%s\n", next_channel, next_msg->string); // TODO: insert channel ID prefix as in SPEC
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void nextChannel(int channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];
    msg_t *message_to_read;
    
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id);
    
    } else if (client->channels[channel_id].subscribed == 0) {
        sprintf(return_msg, "Not subscribed to channel %d\n", channel_id);
    
    } else {
        message_to_read = read_message(channel_id, client);
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


void sendMsg(int channel_id, client_t *client, char *message) {
    // Check for invalid channel
    char return_msg[MAXDATASIZE];

    messages_counts[channel_id] += 1;
    msg_t *msg_struct = &messages_msg[messages_counts[NUMCHANNELS]];
    memcpy(msg_struct->string, message, MAXMESSAGELENGTH);
    msg_struct->user = client->id;
    msg_struct->time = time(NULL);

    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %d\n", channel_id);
        if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
            perror("send");
        }
    }

    // Construct the node for the linked list
    msgnode_t *newhead = node_add(messages[channel_id], msg_struct);

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

}


int bye(client_t *client) {
    printf("Client disconnected.\n");
    close(client->socket); // Close socket on server side
    kill(pids[client->id], SIGTERM);
    return 0; // Required to stop server signal
}


void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining

    // Kill extra process first
    for (int i; i < num_clients; i++) {
        kill(pids[i], SIGTERM);
    }

    // Unmap and close shared memory
    munmap(messages, NUMCHANNELS * sizeof(msgnode_t*));
    munmap(messages_nodes, MAXMESSAGES * sizeof(msgnode_t));
    munmap(messages_msg, MAXMESSAGES * sizeof(msg_t));
    munmap(messages_counts, NUMCHANNELS * sizeof(int));
    close(smfd1);
    close(smfd2);
    shm_unlink("/messages");
    shm_unlink("/counts");

    // Clients should be freed automatically because they're stack variables

    printf("\nHandling the signal gracefully...\n"); 
    // TODO: Deal with every process triggering...

    // Close sockets
    close(sockfd);
    close(new_fd);
    exit(1);
}


msgnode_t* node_add(msgnode_t *head, msg_t *message) {
    // create new node to add to list
    msgnode_t *new = &messages_nodes[messages_counts[NUMCHANNELS]];
    if (new == NULL) {
        return NULL;
    }

    // insert new node
    new->msg = message;
    new->next = head;
    return new;
}


msg_t* read_message(int channel_id, client_t *client) {
    
    msgnode_t *last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t *curr_head = messages[channel_id]; // current head, need to keep moving it back

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


msg_t* get_next_message(int channel_id, client_t *client) {
    msgnode_t *last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t *curr_head = messages[channel_id]; // current head, need to keep moving it back
    if (curr_head == last_read) {
        return NULL;
    }
    while (curr_head->next != last_read) {
        curr_head = curr_head->next;
    }
    return(curr_head->msg);
}


int get_number_unread_messages(int channel_id, client_t *client) {
    msgnode_t *last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t *curr_head = messages[channel_id]; // current head, need to keep moving it back
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