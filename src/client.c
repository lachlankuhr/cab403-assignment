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
#define MAX_THREADS 5

// Globals - Socket and threading
int sockfd;
int next_thread_count = 0;
int livefeed_thread_count = 0;
pthread_t next_threads[MAX_THREADS];
pthread_t livefeed_threads[MAX_THREADS];


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
    int rc;

    for(;;) {
        // Wait for and read user input
        fgets(command, COMMANDSIZE, stdin);

        // Keep a copy of the original command before being broken up for decoding
        strcpy(original_command, command);

        // Command decoding
        // Reset the variables
        channel_id = -1;
        decode_command(command, &channel_id);

        // STOP fully handled by client
        if (strcmp(command, "STOP") == 0) {
            // Terminate any running extra threads
            closeThreads();

        // BYE fully handled by client
        } else if (strcmp(command, "BYE") == 0) {
            closeConnection();

        // NEXT client handles thread creation and sends request to server
        } else if (strcmp(command, "NEXT") == 0) {
            // Create thread
            next_thread_count++;
            long channel = channel_id;

            if ((rc = pthread_create(&next_threads[next_thread_count-1],  
                NULL, nextThreadFunc, (void *)channel))) {
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }

        // LIVEFEED client handles thread creation and NEXT polling loop
        } else if (strcmp(command, "LIVEFEED") == 0) {
            // Create thread
            livefeed_thread_count++;
            long channel = channel_id;

            if ((rc = pthread_create(&livefeed_threads[livefeed_thread_count-1], 
                NULL, livefeedThreadFunc, (void *)channel))) {
                printf("ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }

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
}


int setClientPort(int argc, char ** argv) {
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        exit(-1);
    } else {
        return atoi(argv[2]); // Port number
    }
}


void decode_command(char* command, int *channel_id) {
    if (strtok(command, "\n") == NULL) { // No command
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
        
        if (sep == endptr || errno == EINVAL || (errno != 0 && channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
            *channel_id = -1;
        }
        
    } 
}


void* nextThreadFunc(void *channel) {
    // Send the command
    char command[100];
    long channel_id = (long)channel;
    char buf[MAXDATASIZE];

    if (channel_id == -1) {
        sprintf(command, "NEXT");
    } else if (channel_id != -1) {
        sprintf(command, "NEXT %ld", channel_id);
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
    
    next_thread_count--;
    pthread_exit(NULL);
}


void* livefeedThreadFunc(void *channel) {
    char command[100];
    long channel_id = (long)channel;
    char buf[MAXDATASIZE];

    if (channel_id == -1) {
        sprintf(command, "NEXT");
    } else if (channel_id != -1) {
        sprintf(command, "NEXT %ld", channel_id);
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
    }
    livefeed_thread_count--;
    pthread_exit(NULL);
}


void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    closeConnection();
}


void closeThreads() {
    int rc;
    for (int i = 0; i < next_thread_count; i++) {
        printf("Cancelling next thread %d\n", i);
        if ((rc = pthread_cancel(next_threads[i]))) {
            printf("Failed to cancel thread #%d: %d\n", i, rc);
            exit(-1);
        }
    }
    for (int i = 0; i < livefeed_thread_count; i++) {
        printf("Cancelling livefeed thread %d\n", i);
        if ((rc = pthread_cancel(livefeed_threads[i]))) {
            printf("Failed to cancel thread #%d: %d\n", i, rc);
            exit(-1);
        }
    }
    next_thread_count = 0;
    livefeed_thread_count = 0;
}


void closeConnection() {
    printf("\nClosing client...\n");
    closeThreads();

    char command[MAXDATASIZE];
    sprintf(command, "BYE");

    // Inform server of exit command so it can implement BYE procedure
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
        exit(1);
    }
    // Close socket
    close(sockfd);

    // Close parent thread and thus all children (MUST be last)
    pthread_exit(NULL);
}