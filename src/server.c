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
#include <pthread.h> 
#include "server.h"

#define BACKLOG 5              // How many pending connections queue will hold
#define MAXDATASIZE 2048
#define MAXMESSAGES 1000
#define MAXMESSAGELENGTH 1024
#define NUMCHANNELS 256 // 0 to 255
#define MAXCLIENTS 20


// Global variables
int sockfd, new_fd;       // Listen on sock_fd, new connection on new_fd
char buf[MAXDATASIZE];    // TODO: Localise... Craps itself when I (A) try
pid_t pids[MAXCLIENTS];
int clients_status[MAXCLIENTS];
int clients_active = 0;

// Shared memory address shortcut pointers
msgnode_t **messages;       // Array of pointers to msg_t node heads
msgnode_t *messages_nodes;  // Array of all msg_t nodes
msg_t *messages_msg;        // Array of all msg_t messages
int *messages_counts;       // Array of msg counts by channel, last index is total

int smfd1, smfd2, smfd3, smfd4;          // SHM handle IDs
pthread_rwlock_t rwlock_messages;
pthread_rwlock_t rwlock_counts;

int main(int argc, char **argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);
    signal(SIGCHLD, handleSIGCHLD);
    setupSharedMem();
    
    // Start the server
    startServer(argc, argv);
    struct sockaddr_in client_addr;
    socklen_t sin_size;

    while (1) {
        // Kill all zombie processes
        int stat;
        while(waitpid(-1, &stat, WNOHANG) > 0);

        // Keep checking for new connections
        sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr*)&client_addr, &sin_size)) == -1) {
			perror("accept");
		}
		printf("Server: got connection from %s\n", inet_ntoa(client_addr.sin_addr));
        
        if (clients_active+1 > MAXCLIENTS) {
            printf("Maximum client connections reached, please restart\n");
            // Respond to client with welcome and choose client ID
            char msg[MAXDATASIZE] = "Maximum clients reached, connection declined\n";

            if (send(new_fd, msg, MAXDATASIZE, 0) == -1) {
                perror("send");
            }
            continue;
        }

        pids[clients_active] = fork();
        if (pids[clients_active] == 0) {
            signal(SIGINT, childClose);
            clients_active++;
            clients_status[clients_active-1] = 1;
            client_t client;
            client_setup(&client, clients_active-1);
            //signal(SIGINT, handleChildSIGINT);
            client_processing(&client, pids[clients_active-1]); // Loops

        } else if (pids[clients_active] > 0) { 
            // Parent continues to look for new connections in above while loop section.
            clients_active++;

        } else {
            // Fork failed
            printf("fork() failed\n");
            return -1;
        }
    } // Can't leave while loop without SIGINT
    return 0;
}


void setupSharedMem() {

    size_t size1 = NUMCHANNELS * sizeof(msgnode_t*);
    size_t size2 = MAXMESSAGES * sizeof(msgnode_t);
    size_t size3 = MAXMESSAGES * sizeof(msg_t);
    size_t size4 = (NUMCHANNELS+1) * sizeof(int);

    // Create a shared memory objects
    smfd1 = shm_open("/messages", O_RDWR | O_CREAT, 0600);
    smfd2 = shm_open("/messagesnodes", O_RDWR | O_CREAT, 0600);
    smfd3 = shm_open("/messagesmsg", O_RDWR | O_CREAT, 0600);
    smfd4 = shm_open("/counts", O_RDWR | O_CREAT, 0600);

    // Resize to fit
    ftruncate(smfd1, size1);
    ftruncate(smfd2, size2);
    ftruncate(smfd3, size3);
    ftruncate(smfd4, size4);

    // Map objects
    messages = mmap(NULL, size1, PROT_READ | PROT_WRITE, MAP_SHARED, smfd1, 0); // Must be in SHM (by experimentation)
    messages_nodes = mmap(NULL, size2, PROT_READ | PROT_WRITE, MAP_SHARED, smfd2, 0);
    messages_msg = mmap(NULL, size3, PROT_READ | PROT_WRITE, MAP_SHARED, smfd3, 0);
    messages_counts = mmap(NULL, size4, PROT_READ | PROT_WRITE, MAP_SHARED, smfd4, 0);

    // Initalise message list heads
    for (int i = 0; i < NUMCHANNELS; i++) {
        messages[i] = NULL;
    }

    for (int i = 0; i < MAXMESSAGES; i++) {
        messages_nodes[i].msg = NULL;
        messages_nodes[i].next = NULL;
    }


    // Initalise counts of messages sent to channels. Last index is total.
    for (int i = 0; i < NUMCHANNELS+1; i++) {
        messages_counts[i] = 0;
    }

    pthread_rwlock_init(&rwlock_messages, NULL);
    pthread_rwlock_init(&rwlock_counts, NULL);
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

    for (int i = 0; i < clients_active; i++) {
        clients_status[i] = 0;
    }
    
	printf("Server is listening on port: %i.\n", port_number);
}


int setServerPort(int argc, char **argv) {
    if (argc != 2) {
        return 12345;
    } else {
        if (isNumber(argv[1])) {
            return atoi(argv[1]); // Port number
        } else {
            printf("Please enter a valid port number.\n");
            exit(-1);
        }
    }
}


void client_setup(client_t *client, int client_id) {
    //Setup new client
    client->id = client_id; // set client id
    client->socket = new_fd; // set socket
    for (int i = 0; i < NUMCHANNELS; i++) { // Set subscribed channels
        client->channels[i].subscribed = 0;
        client->channels[i].read = 0;
    }

    // Respond to client with welcome and choose client ID
    char msg[1024] = "Welcome! Your client ID is ";
    char id[5];

    sprintf(id, "%d.\n", client_id);
    strcat(msg, id);

    if (send(client->socket, msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void client_processing(client_t *client, pid_t current_process) {
    // What each process loops around while client active
    long channel_id;
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

        decode_command(client, command, &channel_id, message);

        // Handle commands
        if (strcmp(command, "SUB") == 0) {
            subscribe(channel_id, client);

        } else if (strcmp(command, "CHANNELS") == 0) {
            channels(client);

        } else if (strcmp(command, "UNSUB") == 0 && channel_id != -1) {
            unsubscribe(channel_id, client);

        } else if (strcmp(command, "NEXT") == 0) {
            if (channel_id == -1) {
                next(client);
            } else {
                nextChannel(channel_id, client);
            }

        } else if (strcmp(command, "SEND") == 0) {
            sendMsg(channel_id, client, message);

        } else if (strcmp(command, "BYE") == 0 && channel_id == -1) {
            run = bye(client/*, current_process*/);

        } else {
            if (strtok(command, "\n") != NULL) { // Important to prevent the no command entered and invalid command printing
                if (send(client->socket, "Invalid command\n", MAXDATASIZE, 0) == -1) {
                    perror("send");
                }
            }
        }
    }
}


void decode_command(client_t *client, char *command, long *channel_id, char *message) {
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

    if (sep != NULL) {  // Get channel ID
        // Reset errno to 0 before call 
        errno = 0;

        // Call to strtol assigning return to number
        char *endptr = NULL;
        *channel_id = strtol(sep, &endptr, 10);
        
        if (sep == endptr || errno == EINVAL || (errno != 0 && *channel_id == 0) 
        || (errno == 0 && sep && *endptr != 0) || errno == ERANGE) {
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


void subscribe(long channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %ld.\n", channel_id);
    
    } else if (client->channels[channel_id].subscribed == 1) {
        sprintf(return_msg, "Already subscribed to channel %ld.\n", channel_id);
    
    } else {
        sprintf(return_msg, "Subscribed to channel %ld.\n", channel_id);
        client->channels[channel_id].subscribed = 1;

        pthread_rwlock_rdlock(&rwlock_messages); // Read lock
        client->read_msg[channel_id] = messages[channel_id]; // Point client at last message in the channel
        pthread_rwlock_unlock(&rwlock_messages); // Unlock
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void channels(client_t *client) {
    char return_msg[MAXDATASIZE];
    strcpy(return_msg, "");
    
    for (long channel_id = 0; channel_id < NUMCHANNELS; channel_id++) {
        if (client->channels[channel_id].subscribed == 1) {

            pthread_rwlock_rdlock(&rwlock_counts); // Read lock
            sprintf(buf, "%ld\t%ld\t%d\t%d\n", channel_id, (long)messages_counts[channel_id],
                client->channels[channel_id].read, get_number_unread_messages(channel_id, client));
            pthread_rwlock_unlock(&rwlock_counts); // Unlock
            strcat(return_msg, buf);
        }
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void unsubscribe(long channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %ld.\n", channel_id);
    
    } else if (client->channels[channel_id].subscribed == 0) {
        sprintf(return_msg, "Not subscribed to channel %ld\n", channel_id);
    
    } else {
        sprintf(return_msg, "Unsubscribed from channel %ld\n", channel_id);
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

    for (long channel_id = 0; channel_id <  NUMCHANNELS; channel_id++) {
        if (client->channels[channel_id].subscribed == 1) {
            client_subscribed_to_any_channel = 1;

            pthread_rwlock_wrlock(&rwlock_messages); // Write lock
            msg_t *message = get_next_message(channel_id, client); // just get and not move head forward
            pthread_rwlock_unlock(&rwlock_messages); // Unlock

            pthread_rwlock_rdlock(&rwlock_messages); // Read lock
            // Make sure the oldest available message is returned
            if (message != NULL) {
                if (message->time < min_time) {
                    min_time = message->time;
                    next_msg = message;
                    next_channel = channel_id;
                }
            }
            pthread_rwlock_unlock(&rwlock_messages); // Unlock
        }
    }

    if (client_subscribed_to_any_channel == 0) {
        sprintf(return_msg, "Not subscribed to any channels.\n");
    
    } else if (next_msg == NULL) {
        return_msg[0] = 0; // Empty string
    
    } else {
        pthread_rwlock_rdlock(&rwlock_messages); // Read lock
        read_message(next_channel, client); // Move head foward
        pthread_rwlock_unlock(&rwlock_messages); // Unlock
        sprintf(return_msg, "%d:%s\n", next_channel, next_msg->string);  
    }
    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void nextChannel(long channel_id, client_t *client) {
    char return_msg[MAXDATASIZE];
    msg_t *message_to_read;
    
    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %ld\n", channel_id);
    
    } else if (client->channels[channel_id].subscribed == 0) {
        sprintf(return_msg, "Not subscribed to channel %ld\n", channel_id);
    
    } else {
        pthread_rwlock_wrlock(&rwlock_messages); // Write (and read) lock
        message_to_read = read_message(channel_id, client);
        if (message_to_read == NULL) {
            return_msg[0] = 0;
        } else {
            sprintf(return_msg, "%ld:%s\n", channel_id, message_to_read->string); 
        }
        pthread_rwlock_unlock(&rwlock_messages); // Unlock
    }

    if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
}


void sendMsg(long channel_id, client_t *client, char *message) {
    // Check for invalid channel
    char return_msg[MAXDATASIZE];
    
    pthread_rwlock_rdlock(&rwlock_messages); // Read lock
    msg_t *msg_struct = &messages_msg[messages_counts[NUMCHANNELS]]; // Points
    memcpy(msg_struct->string, message, MAXMESSAGELENGTH);
    msg_struct->user = client->id;
    msg_struct->time = time(NULL);
    pthread_rwlock_unlock(&rwlock_messages); // Unlock

    // Increment channel ID count and total count


    if (channel_id < 0 || channel_id > 255) {
        sprintf(return_msg, "Invalid channel: %ld\n", channel_id);
        if (send(client->socket, return_msg, MAXDATASIZE, 0) == -1) {
            perror("send");
        }
    }
    pthread_rwlock_wrlock(&rwlock_counts); // Write lock to message counts shm
    messages_counts[channel_id]++;
    messages_counts[NUMCHANNELS]++;
    pthread_rwlock_unlock(&rwlock_counts); // Unlock

    // Construct the node for the linked list
    pthread_rwlock_wrlock(&rwlock_messages); // Write lock to messages shm
    msgnode_t *newhead = node_add(messages[channel_id], msg_struct);

    if (newhead == NULL) {
        printf("Memory allocation error");
        exit(EXIT_FAILURE);
    }
    messages[channel_id] = newhead;
    pthread_rwlock_unlock(&rwlock_messages); // Unlock
    
    // Send back nothing because otherwise its blocking
    if (send(client->socket, "", MAXDATASIZE, 0) == -1) {
        perror("send");
    }

}


int bye(client_t *client) {
    childClose();
    return 0; // Required to stop server signal
}

void childClose() {
    pthread_rwlock_destroy(&rwlock_messages);
    pthread_rwlock_destroy(&rwlock_counts);

    close(new_fd);
    printf("Client disconnected\n");
    kill(getpid(), SIGTERM); // Child processes close themselves
    exit(1);
}


void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining

    pthread_rwlock_destroy(&rwlock_messages);
    pthread_rwlock_destroy(&rwlock_counts);

    // Unmap and close shared memory
    munmap(messages, NUMCHANNELS * sizeof(msgnode_t*));
    munmap(messages_nodes, MAXMESSAGES * sizeof(msgnode_t));
    munmap(messages_msg, MAXMESSAGES * sizeof(msg_t));
    munmap(messages_counts, (NUMCHANNELS+1) * sizeof(int));
    close(smfd1);
    close(smfd2);
    close(smfd3);
    close(smfd4);
    shm_unlink("/messages");
    shm_unlink("/messagesnodes");
    shm_unlink("/messagesmsg");
    shm_unlink("/counts");

    // Clients freed automatically as stack variables

    printf("\nServer closing...\n"); 

    // Close sockets
    close(sockfd); close(new_fd);

    // Kill all process
    kill(0, SIGTERM);
    exit(1);
}
// Linked list / data structure methods //
// No locks in the following as all locks surround the calls to the below functions

void handleSIGCHLD(int signum) {
  int stat;

  /*Kills all the zombie processes*/
  while(waitpid(-1, &stat, WNOHANG) > 0);
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


msg_t* read_message(long channel_id, client_t *client) {
    
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


msg_t* get_next_message(long channel_id, client_t *client) {

    msgnode_t *last_read = client->read_msg[channel_id]; // current last read message
    msgnode_t *curr_head = messages[channel_id]; // current head, need to keep moving it back

    if (curr_head == last_read) {
        return NULL;
    }
    while (curr_head->next != last_read) {
        curr_head = curr_head->next;            // Problem line
    }
    return(curr_head->msg);
}


int get_number_unread_messages(long channel_id, client_t *client) {
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