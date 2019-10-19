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

#define MAXDATASIZE 30000 // Needs to be high
#define COMMANDSIZE 50
#define MAX_THREADS 3

// Globals - Socket and threading
int sockfd;
int thread_states[MAX_THREADS];
pthread_t threads[MAX_THREADS];


int main(int argc, char **argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Start the client
    startClient(argc, argv);
    
    // Command components
    char command[COMMANDSIZE];
    char original_command[COMMANDSIZE];
    char buf[MAXDATASIZE];
    int next_neg_one;
    int channel_id;
    int rc;

    for(;;) {
        // Wait for and read user input
        fgets(command, COMMANDSIZE, stdin);

        // Keep a copy of the original command before being broken up for decoding
        strcpy(original_command, command);

        // Command decoding
        // Reset the variables
        channel_id = -1;
        next_neg_one = 0;
        decode_command(command, &channel_id, &next_neg_one);

        // STOP fully handled by client
        if (strcmp(command, "STOP") == 0) {
            // Terminate any running extra threads
            closeThreads();

        // BYE fully handled by client
        } else if (strcmp(command, "BYE") == 0) {
            closeConnection();

        } else if (strcmp(command, "UNSUB -1") == 0) {
            printf("Invalid channel: -1\n");
            continue;
        
        // NEXT client handles thread creation and sends request to server
        } else if (strcmp(command, "NEXT") == 0) {
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
                break;
            }

            // Create thread
            thread_args_t args;
            args.channel = channel_id;
            args.id = next_thread_id;

            if ((rc = pthread_create(&threads[next_thread_id],  
                NULL, nextThreadFunc, (void *)&args))) {
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }
            thread_states[next_thread_id] = 1;

        // LIVEFEED client handles thread creation and NEXT polling loop
        } else if (strcmp(command, "LIVEFEED") == 0) {
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

            if ((rc = pthread_create(&threads[next_thread_id], 
                NULL, livefeedThreadFunc, (void *)&args))) {
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }
            thread_states[next_thread_id] = 1;

        // All other commands send to server
        } else { 
            // Do the usual send and receive
            if (send(sockfd, original_command, MAXDATASIZE, 0) == -1) {
                perror("send");
            }
            // Receive the response
            int numbytes;  
            if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
                perror("recv.");
            }
            buf[numbytes] = '\0';
            printf("%s", buf);
        }
    }
    pthread_exit(NULL);
    return 0;
}


void startClient(int argc, char **argv) {
    // Set the client's port number
    int port_number = setClientPort(argc, argv);
    struct hostent *he;
    char buf[MAXDATASIZE];

    if ((he=gethostbyname(argv[1])) == NULL) {  // Get the server info
		herror("gethostbyname");
		exit(1);
	}

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

    // Setup socket then connect
    struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_number);
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8); // Zero the rest of the struct

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

    // Receive the startup message
    int numbytes;  
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';
	printf("%s", buf);

    // Initisialse thread states
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_states[i] = 0;
    }
}


int setClientPort(int argc, char **argv) {
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        exit(-1);
    } else {
        return atoi(argv[2]); // Port number
    }
}


void decode_command(char *command, int *channel_id, int *next_neg_one) {
    if (strtok(command, "\n") == NULL) { // No command
        return;
    }
    if (strcmp("NEXT -1", command) == 0) *next_neg_one = 1;

    char *sep = strtok(command, " ");   // Separate command from arguments

    sep = strtok(NULL, " ");
    if (sep != NULL) {  // Get channel ID
        // Reset errno to 0 before call 
        errno = 0;

        // Call to strtol assigning return to number 
        char *endptr = NULL;
        *channel_id = strtol(sep, &endptr, 10);
        
        if (sep == endptr || errno == EINVAL || (errno != 0 && channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
            *channel_id = -1;
        }
        
    } 
}


void* nextThreadFunc(void *args_sent) {
    struct thread_args *args = (struct thread_args *)args_sent;

    // Send the command
    char command[100];
    char buf[MAXDATASIZE];
    int channel_id = args->channel;

    if (channel_id == -1) {
        sprintf(command, "NEXT");
    } else if (channel_id != -1) {
        sprintf(command, "NEXT %d", channel_id);
    }
    
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
    // Receive the response
    int numbytes;  
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv.");
    }
    buf[numbytes] = '\0';
    printf("%s", buf);
    
    thread_states[args->id] = 0;
    pthread_exit(NULL);
}


void* livefeedThreadFunc(void *args_sent) {
    struct thread_args *args = (struct thread_args *)args_sent;

    char command[100];
    char buf[MAXDATASIZE];
    int channel_id = args->channel;

    if (channel_id == -1) {
        sprintf(command, "NEXT");
    } else if (channel_id != -1) {
        sprintf(command, "NEXT %d", channel_id);
    }
    
    while (1) {
        // Send the command
        if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
            perror("send");
        }
        // Receive the response
        int numbytes;  
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
            perror("recv.");
        }
        buf[numbytes] = '\0';
        printf("%s", buf);
        
        if (strncmp(buf, "Not", 3) == 0) {
            break;
        }

        sleep(0.5);
    } // How does this exit?

    thread_states[args->id] = 0;
    pthread_exit(NULL);
}


void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    closeConnection();
}


void closeThreads() {
    int rc;

    for (int i = 0; i < MAX_THREADS; i++) {

        if (thread_states[i] == 1) {
            printf("Cancelling thread %d\n", i);
            if ((rc = pthread_cancel(threads[i]))) {
                printf("Failed to cancel thread or already cancelled #%d: %d\n", i, rc);
                exit(-1);
            }
            thread_states[i] = 0;
        }
    }
}


void closeConnection() {
    
    // Inform user
    printf("\nClosing client...\n");
    
    // Close threads
    closeThreads();

    // Inform server of exit command so it can implement BYE procedure
    char command[MAXDATASIZE];
    sprintf(command, "BYE");
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
        exit(1);
    }
    
    // Close socket
    close(sockfd);

    // Close parent thread and thus all children (MUST be last)
    pthread_exit(NULL);
}