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

#define MAXDATASIZE 1024
#define COMMANDSIZE 50

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
        if (strncmp(command_name, "SUB", 3) == 0 && channel_id != -1) {
            printf("SUB command entered.\n");
        } else if (strncmp(command_name, "CHANNELS", 8) == 0) {
            printf("CHANNELS command entered.\n");
        } else if (strncmp(command_name, "UNSUB", 5) == 0 && channel_id != -1) {
            printf("UNSUB command entered.\n");
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

	server_addr.sin_family = AF_INET;      /* host byte order */
	server_addr.sin_port = htons(port_number);    /* short, network byte order */
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8);     /* zero the rest of the struct */

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