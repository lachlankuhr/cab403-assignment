#include <stdio.h> 
#include <stdlib.h> 
#include <string.h>
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <unistd.h>
#include "client.h"

void subscribe(int channel_id, int *subscribed_channels);
void channels(int* subscribed_channels);
void unsubscribe(int channel_id, int *subscribed_channels);

#define MAXDATASIZE 1024
#define NUMCHANNELS 255
#define COMMANDSIZE 50  // This will need to be considered
#define MAX_INPUT 3

// Global variables
int port_number; 
int sockfd;  
struct hostent *he;
struct sockaddr_in server_addr;

// Receiving data
int numbytes;  
char buf[MAXDATASIZE];

int main(int argc, char ** argv) {
    startClient(argc, argv);
    // Abitarily using sizes
    char command[COMMANDSIZE];
    char * command_name;
    char * msg;
    int channel_id;
    int subscribed_channels[NUMCHANNELS]; // array for subscribed channels - store client side and client can make requests to server
    for (int i = 0; i < NUMCHANNELS; i++) {
        subscribed_channels[i] = 0; // initialise subscribed channels array to 0
    }
    char * input[MAX_INPUT];

    // Continue to look for new comamnds
    for(;;) {
        // Reset the variables
        command_name = "";
        channel_id = -1;
        msg = "";

        // Wait for and read user input
        printf("\nCommand: ");
        fgets(command, COMMANDSIZE, stdin);

        if (strtok(command, "\n") == NULL) { // No command
            printf("No command entered\n");
            continue;
        }

        char* sep = strtok(command, " ");   // Separate command from arguments
        if (sep != NULL) command_name = sep;

        sep = strtok(NULL, " ");
        if (sep != NULL) {                           // Get channel ID
            printf("%s",sep);
            channel_id = atoi(sep);
            if (channel_id == 0) {
                printf("Invalid channel ID\n");      //TODO: Proper validation
                continue;
            }
            sep = strtok(NULL, ""); // this SHOULD be "" instead of " ". It gets the rest of the message.
        } 
        // The message is considered to be the rest of command (including
        // spaces) after the second parameter. 
        // Checking for too many parameters should be done later and 
        // dependent on what the command entered was. 

        if (sep != NULL) {                  // Get msg (for send only)
            msg = sep;                      // TODO: Msg format validation
        }

        printf("Command entered: %s\n", command_name);
        printf("Channel ID entered: %d\n", channel_id);
        //printf("Msg entered: %s\n", msg);


        if (strcmp(command_name, "SUB") == 0 && channel_id  -1) {
            subscribe(channel_id, subscribed_channels);

        } else if (strcmp(command_name, "CHANNELS") == 0) {
            channels(subscribed_channels);

        } else if (strcmp(command_name, "UNSUB") == 0 && channel_id != -1) {
            unsubscribe(channel_id, subscribed_channels);

        } else if (strcmp(command_name, "NEXT") == 0 && channel_id != -1) {
            printf("NEXT command with channel ID entered.\n");

        } else if (strcmp(command_name, "NEXT") == 0 && channel_id == -1) {
            printf("NEXT command without channel ID entered.\n");

        } else if (strcmp(command_name, "LIVEFEED") == 0 && channel_id != -1) {
            printf("LIVEFEED command with channel ID entered.\n");

        } else if (strcmp(command_name, "LIVEFEED") == 0 && channel_id == -1) {
            printf("LIVEFEED command without channel ID entered.\n");

        } else if (strcmp(command_name, "SEND") == 0) {
            printf("SEND command entered.\n");
            printf("%s\n", msg);

        } else if (strcmp(command_name, "BYE") == 0 && channel_id == -1) {
            printf("BYE command entered.\n");

        } else {
            printf("Command entered is not valid.\n");
        }

    }

    close(sockfd);
    return 0;
}

// Subscribe to channel - I think this is okay to just be stored on the client
void subscribe(int channel_id, int *subscribed_channels) {
    // check for invalid id
    if (channel_id < 0 || channel_id > NUMCHANNELS) {
        printf("Invalid channel: %d.\n", channel_id);
        return;
    }
    // check if already subscribed to channel
    if (subscribed_channels[channel_id] == 1) {
        printf("Already subscribed to channel %d.\n", channel_id);
    } else { // subscribe to channel
        subscribed_channels[channel_id] = 1;
        printf("Subscribed to channel %d.\n", channel_id);
    }
    return;
}

// For now, just get this printing out the channels
void channels(int* subscribed_channels) {
    for (int i = 0; i < NUMCHANNELS; i++) {
        if (subscribed_channels[i] == 1) {
            printf("%i\tnum_messages\tread_messages\tunread_messages\n", i); // num_messages, read_messages etc placeholder values
        }
    }
}

void unsubscribe(int channel_id, int *subscribed_channels) {
    // check for invalid id
    if (channel_id < 0 || channel_id > NUMCHANNELS) {
        printf("Invalid channel: %d.\n", channel_id);
        return;
    }
    // check if already subscribed to channel
    if (subscribed_channels[channel_id] == 0) {
        printf("Not subscribed to channel %d.\n", channel_id);
    } else { // unsubscribe to channel
        subscribed_channels[channel_id] = 0;
        printf("Unsubscribed from channel %d.\n", channel_id);
    }
    return;
}

void setClientPort(int argc, char ** argv) {
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        exit(-1);
    } else {
        port_number = atoi(argv[2]);
    }
}

void startClient(int argc, char ** argv) {
    // Set the client's port number
    setClientPort(argc, argv);
    
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