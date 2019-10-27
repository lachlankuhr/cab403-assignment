#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <signal.h>
#include <netdb.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <errno.h> 
#include <pthread.h>
#include "client.h"
#include "data.h"

#define MAXDATASIZE 2048    // Server ensures messages are 1024 max
#define COMMANDSIZE 50
#define MAX_THREADS 20

// Globals - Socket and threading
int sockfd;                     // Connection to server
int thread_states[MAX_THREADS]; // Status of any threads created
pthread_t threads[MAX_THREADS]; // Threads created for NEXT or LIVEFEED
pthread_mutex_t socket_lock; // Setup socket lock to ensure clients synced


int main(int argc, char **argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Start the client
    startClient(argc, argv);
    
    // Command components
    char command[COMMANDSIZE];
    char original_command[COMMANDSIZE];
    char buf[MAXDATASIZE];
    int channel_id;
    int next_neg_one;   // Set to 1 if '-1' was truly entered as channel_id. As -1 is used for errors
    int rc;

    for(;;) {
        // Wait for and read user input
        fgets(command, COMMANDSIZE, stdin);

        // Keep a copy of the original command before being decoding
        strcpy(original_command, command);

        // Command decoding
        // Reset the variables
        channel_id = -1;
        next_neg_one = 0;
        decode_command(command, &channel_id, &next_neg_one);

        // STOP fully handled by client
        if (strcmp(command, "STOP") == 0) {
            // Terminate any running LIVEFEED or NEXT threads
            closeThreads();

        // BYE handled by client then message sent to server
        } else if (strcmp(command, "BYE") == 0) {
            closeConnection();

        // Edge case check
        } else if (strcmp(command, "UNSUB -1") == 0) {
            printf("Invalid channel: -1\n");
            continue;
        
        // NEXT client handles thread creation and sends request to server
        } else if (strcmp(command, "NEXT") == 0) {
            // Edge case check
            if (next_neg_one == 1) {
                printf("Invalid channel: -1\n");
                continue;
            }
            // Find first empty pid in thread array
            int next_thread_id = -1;
            for (int i = 0; i < MAX_THREADS; i++) {
                if (thread_states[i] == 0) {
                    next_thread_id = i;
                    break;
                }
            }
            if (next_thread_id == -1) {
                printf("No threads remain available to execute command\n");
                continue;
            }

            // Create thread
            thread_args_t args;
            args.channel = channel_id;
            args.id = next_thread_id;
            thread_states[next_thread_id] = 1; // Update state tracker

            if ((rc = pthread_create(&threads[next_thread_id],  
            NULL, nextThreadFunc, (void *)&args))) {
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }

        // LIVEFEED client handles thread creation and NEXT polling loop
        } else if (strcmp(command, "LIVEFEED") == 0) {
            // Edge case check
            if (next_neg_one == 1) {
                printf("Invalid channel: -1\n");
                continue;
            }

            // Find first empty pid in thread array
            int next_thread_id = -1;
            for (int i = 0; i < MAX_THREADS; i++) {
                if (thread_states[i] == 0) {
                    next_thread_id = i;
                    break;
                }
            }
            if (next_thread_id == -1) {
                printf("No threads are available\n");
                continue;
            }

            // Create thread
            thread_args_t args;
            args.channel = channel_id;
            args.id = next_thread_id;
            thread_states[next_thread_id] = 1; // Update state tracker

            if ((rc = pthread_create(&threads[next_thread_id], 
                NULL, livefeedThreadFunc, (void *)&args))) {
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }

        // All other commands are fully handled by the server and a response waited upon.
        // This response can be an empty response or an error allowing the client to continue.
        } else { 
            // Send client command entered
            pthread_mutex_lock(&socket_lock);
            if (send(sockfd, original_command, MAXDATASIZE, 0) == -1) {
                perror("send");
            }
            // Receive the response
            int numbytes;  
            if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv.");
            }
            pthread_mutex_unlock(&socket_lock);
            buf[numbytes] = '\0';
            printf("%s", buf);
        }
    }
    pthread_exit(NULL); // In case the loop exits
    return 0;
}


void startClient(int argc, char **argv) {
    // Set the client's port number
    int port_number = setClientPort(argc, argv);
    struct hostent *he;
    char buf[MAXDATASIZE];

    // Get the server info
    if ((he=gethostbyname(argv[1])) == NULL) { 
		herror("gethostbyname");
		exit(1);
	}

    // Setup socket then connect
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}
    struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_number);
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8); // Zero the rest of the struct

    // Connect socket
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

    // Receive the startup message
    int numbytes;
    pthread_mutex_lock(&socket_lock);
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}
    pthread_mutex_unlock(&socket_lock);

	buf[numbytes] = '\0';
	printf("%s", buf);

    // Check if connection refused due to exceeding max clients
    if (strncmp("Maximum\n", buf, 7) == 0) {
        closeConnection();
    }

    // Initisialse thread states array to inactive
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_states[i] = 0;
    }

    // Setup socket lock to ensure clients synced
    pthread_mutex_init(&socket_lock, NULL);
}


int setClientPort(int argc, char **argv) {
    // Correct number of arguments entered?
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        exit(-1);
    } else {
        // Valid port number? If hostname invalid it will provide its own warning.
        if (isNumber(argv[2])) {
            return atoi(argv[2]); // Port number
        } else {
            printf("Please enter a valid port number.\n");
            exit(-1);
        }
    }
}


void decode_command(char *command, int *channel_id, int *next_neg_one) {
    // Check if no command entered at all
    if (strtok(command, "\n") == NULL) {
        return;
    }
    // Check for edge case
    if (strcmp("NEXT -1", command) == 0) *next_neg_one = 1;

    // Separate command from arguments
    char *sep = strtok(command, " ");

    // Separate channel ID from any remainder of message
    sep = strtok(NULL, " ");
    if (sep != NULL) {
        // Reset errno to 0 before call 
        errno = 0;

        // Call to strtol assigning return to number 
        char *endptr = NULL;
        *channel_id = strtol(sep, &endptr, 10);
        
        // Error checking
        if (sep == endptr || errno == EINVAL || (errno != 0 && channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
            *channel_id = -1;
        }
        // Else channel ID was assigned a valid number
    } 
}


void* nextThreadFunc(void *args_sent) {
    // Receive arguments sent
    struct thread_args *args = (struct thread_args *)args_sent;

    char command[100];
    char buf[MAXDATASIZE];
    int channel_id = args->channel;

    // Determine if a NEXT or a NEXT <ID> command was entered
    if (channel_id == -1) {
        sprintf(command, "NEXT");
    } else if (channel_id != -1) {
        sprintf(command, "NEXT %d", channel_id);
    }
    
    // Send the correct NEXT command to server
    pthread_mutex_lock(&socket_lock);        // Lock
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
    // Receive the response
    int numbytes;  
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv.");
    }
    pthread_mutex_unlock(&socket_lock);      // Unlock

    buf[numbytes] = '\0';
    printf("%s", buf);
    
    // Update the thread a finished and exit
    thread_states[args->id] = 0;
    pthread_exit(NULL);
}


void* livefeedThreadFunc(void *args_sent) {
    // Receive arguments sent
    struct thread_args *args = (struct thread_args *)args_sent;
    printf("Thing: %d\n", args->id);

    char command[100];
    char buf[MAXDATASIZE];
    int channel_id = args->channel;

    // Determine if a LIVEFEED or a LIVEFEED <ID> command was entered.
    // Send the corresponding NEXT command on loop.
    if (channel_id == -1) {
        sprintf(command, "NEXT");
    } else if (channel_id != -1) {
        sprintf(command, "NEXT %d", channel_id);
    }

    // Loop send the command
    int numbytes;  
    while (1) {
        pthread_mutex_lock(&socket_lock);        // Lock
        if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
            perror("send");
        }
        // Receive the response
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
            perror("recv.");
        }
        pthread_mutex_unlock(&socket_lock);        // Unlock

        buf[numbytes] = '\0';
        printf("%s", buf);
        
        // Stop if an error received back
        if (strncmp(buf, "Not", 3) == 0) {
            thread_states[args->id] = 0;
            printf("Livefeed thread %d cancelled\n", args->id);
            break;
        }
        sleep(0.1); // Stop resource hogging
    }

    // Update the thread a finished and exit if the loop breaks.
    // Generally LIVEFEED will be stoped with the "STOP" command.
    pthread_exit(NULL);
}


void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    // Close connection on Ctrl+c use
    closeConnection();
}


void closeThreads() {
    int rc;

    // Close all active threads when STOP is entered or client closed
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_states[i] == 1) {
            printf("Cancelling thread %d\n", i);
            if ((rc = pthread_cancel(threads[i]))) {
                printf("Failed to cancel thread or already cancelled #%d, error: %d\n", i, rc);
                exit(-1);
            }
            thread_states[i] = 0;
        }
    }
}


void closeConnection() {
    
    // Close threads
    closeThreads();

    // Inform user
    printf("\nClosing client...\n");

    // Inform server of exit command so it can implement BYE procedure
    char command[MAXDATASIZE];
    sprintf(command, "BYE");

    // No need to lock when closing it
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
        exit(1);
    }
    
    // Server closes the socket
    close(sockfd);

    // Close parent thread and thus all children (MUST be last)
    pthread_exit(NULL);
}