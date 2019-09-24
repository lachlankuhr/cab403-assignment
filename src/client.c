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
#define COMMANDSIZE 50
#define NUMCHANNELS 255

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
    int subscribed_channels[NUMCHANNELS]; // array for subscribed channels
    for (int i = 0; i < NUMCHANNELS; i++) {
        subscribed_channels[i] = 0; // initialise subscribed channels array to 0
    }

    // Continue to look for new comamnds
    while (1) {
        // Reset the variables
        channel_id = -1;

        // Read the input from the user
        fgets(command, COMMANDSIZE, stdin);
        char * sep = strtok(command," ");
        if (sep != NULL) {
            command_name = sep;
            sep = strtok(NULL, " ");
        } 
        if (sep != NULL) {
            channel_id = atoi(sep);
            sep = strtok(NULL, "");
        } else {
            channel_id = -1; // no channel id associated with this command
        }
        if (sep != NULL) {
            channel_id = atoi(sep);
            msg = sep;
        }

        printf("Command entered: %s.\n", command_name);
        printf("Channel ID entered: %i.\n", channel_id);

        // Determine what command was entered
        // Using strncmp instead of strcmp due to the way it is null terminated if no channel number
        // TODO: Get strcmp working (deal with line ending crap), so e.g. SUBS doesn't work.
        if (strncmp(command_name, "SUB", 3) == 0 && channel_id != -1) {
            subscribe(channel_id, subscribed_channels);
        } else if (strncmp(command_name, "CHANNELS", 8) == 0) {
            channels(subscribed_channels);
        } else if (strncmp(command_name, "UNSUB", 5) == 0 && channel_id != -1) {
            unsubscribe(channel_id, subscribed_channels);
        } else if (strncmp(command_name, "NEXT", 4) == 0 && channel_id != -1) {
            printf("NEXT command with channel ID entered.\n");
        } else if (strncmp(command_name, "NEXT", 4) == 0 && channel_id == -1) {
            printf("NEXT command without channel ID entered.\n");
        } else if (strncmp(command_name, "LIVEFEED", 8) == 0 && channel_id != -1) {
            printf("LIVEFEED command with channel ID entered.\n");
        } else if (strncmp(command_name, "LIVEFEED", 8) == 0 && channel_id == -1) {
            printf("LIVEFEED command without channel ID entered.\n");
        } else if (strncmp(command_name, "SEND", 4) == 0) {
            printf("SEND command entered.\n");
            printf("%s\n", msg);
        } else if (strncmp(command_name, "BYE", 3) == 0 && channel_id == -1) {
            printf("BYE command entered.\n");
        } else {
            printf("Command entered doesn't match any.\n");
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
            printf("%i\tnum_messages\tread_messages\tunread_messages\n", i);
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
    
    if ((he=gethostbyname(argv[1])) == NULL) {  // get the server info
		herror("gethostbyname");
		exit(1);
	}

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	server_addr.sin_family = AF_INET;             /* host byte order */
	server_addr.sin_port = htons(port_number);    /* short, network byte order */
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8);            /* zero the rest of the struct */

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}

    // Receive the startup message
    if ((numbytes=recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("%s", buf);

}