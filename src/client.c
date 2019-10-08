#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <signal.h>
#include <netdb.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include <errno.h> 
#include <pthread.h>
#include "client.h"
#include "data.h"

#define MAXDATASIZE 30000
#define NUMCHANNELS 255
#define COMMANDSIZE 50  // This will need to be considered
#define MAX_INPUT 3

// Global variables
int sockfd;
struct hostent *he;
struct sockaddr_in server_addr;
int is_livefeed = 0;

// Receiving data
int numbytes;  
char buf[MAXDATASIZE];

int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handleSIGINT);

    // Start the client
    startClient(argc, argv);

    int subscribed_channels[NUMCHANNELS]; // array for subscribed channels - store client side and client can make requests to server
    for (int i = 0; i < NUMCHANNELS; i++) {
        subscribed_channels[i] = 0; // initialise subscribed channels array to 0
    }
    char * input[MAX_INPUT];


    //pthread_t threads[NUM_THREADS];
    //printf("In main: creating thread %ld\n", t);
    //rc = pthread_create(&threads[t], NULL, PrintHello, (void *)t);
    //if (rc){
    //    printf("ERROR; return code from pthread_create() is %d\n", rc);
    //    exit(-1);
    //}

    // Command components
    char command[COMMANDSIZE];
    char original_command[COMMANDSIZE];
    char * command_name;
    int channel_id;
    char * message;
    for(;;) {
        // Wait for and read user input
        fgets(command, COMMANDSIZE, stdin);

        // Keep a copy of the original command before being broken up for decoding
        strcpy(original_command, command);

        // Command decoding
        // Reset the variables
        command_name = "";
        channel_id = -1;
        decode_command(command, command_name, &channel_id);

        // Spawn the threads here
        if (strcmp(command, "STOP") == 0) {
            // terminate the livenext and next threads
        } else if (strcmp(command, "BYE") == 0) {
            closeConnection();
        } else if (strcmp(command, "NEXT") == 0 && channel_id != -1) {
            nextChannel(original_command);
            // create thread
        } else if (strcmp(command, "NEXT") == 0 && channel_id == -1) {
            // create thread
            next(original_command);

        } else if (strcmp(command, "LIVEFEED") == 0 && channel_id != -1) {
            // create thread
            livefeedChannel(channel_id);

        } else if (strcmp(command, "LIVEFEED") == 0 && channel_id == -1) {
            livefeed();
        } else { // do the usual send and receive  
            // Send command name
            if (send(sockfd, original_command, MAXDATASIZE, 0) == -1) {
                perror("send");
            }
            // Receive the response
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

void nextChannel(char * command) {
    // Send the command
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
    // Receive the response
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv.");
    }
    buf[numbytes] = '\0';

    printf("%s", buf);
}

void next(char * command) {
    // Send the command
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }
    // Receive the response
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("recv.");
    }
    buf[numbytes] = '\0';

    printf("%s", buf);
}

void livefeed() {
    while (1) {
        // Send the command
        if (send(sockfd, "NEXT", MAXDATASIZE, 0) == -1) {
            perror("send");
        }
        // Receive the response
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
            perror("recv.");
        }
        buf[numbytes] = '\0';

        printf("%s", buf);
    }
}

void livefeedChannel(int channel_id) {
    char command[100];
    sprintf(command, "NEXT %i", channel_id);
    while (1) {
        // Send the command
        if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
            perror("send");
        }
        // Receive the response
        if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
            perror("recv.");
        }
        buf[numbytes] = '\0';

        printf("%s", buf);
    }
}


void decode_command(char* command, char * command_name, int * channel_id) {
    if (strtok(command, "\n") == NULL) { // No command
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
        
        if (sep == endptr || errno == EINVAL || (errno != 0 && channel_id == 0) || (errno == 0 && sep && *endptr != 0)) {
            *channel_id = -1;
        }
        
    } 
}

void handleSIGINT(int _) {
    (void)_; // To stop the compiler complaining
    if (!is_livefeed) {
        printf("Handling the signal gracefully for thread %d...\n", 1); //TODO
        closeConnection();
    } else {
        // Exit livefeed as necessary
    }
}

int setClientPort(int argc, char ** argv) {
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        exit(-1);
    } else {
        return atoi(argv[2]); // Port number
    }
}

void startClient(int argc, char ** argv) {
    // Set the client's port number
    int port_number = setClientPort(argc, argv);

    if ((he=gethostbyname(argv[1])) == NULL) {  // Get the server info
		herror("gethostbyname");
		exit(1);
	}

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	server_addr.sin_family = AF_INET;             // Host byte order */
	server_addr.sin_port = htons(port_number);    // Short, network byte order */
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8);            // Zero the rest of the struct */

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

    // Receive the startup message
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("%s", buf);

}

void closeConnection() {
    printf("Closing client...\n");
    char command[MAXDATASIZE];
    sprintf(command, "BYE");
    // Inform server of exit command so it can implement BYE procedure
    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
        exit(1);
    }
    // clear dynamic memory - there is none

    // close threads - when implemented

    // Close socket
    close(sockfd);

    // exit
    exit(1);
}