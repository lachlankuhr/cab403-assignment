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
            sep = strtok(NULL, " ");
        }

        if (sep != NULL) {                  // Get msg (for send only)
            msg = sep;                      // TODO: Msg format validation
            sep = strtok(NULL, " ");
        }

        if (sep != NULL) {                  // Check no format misuse
            printf("Too many commands entered\n");
            continue;
        }

        printf("Command entered: %s\n", command_name);
        printf("Channel ID entered: %d\n", channel_id);
        //printf("Msg entered: %s\n", msg);


        if (strcmp(command_name, "SUB") == 0 && channel_id  -1) {
            printf("SUB command entered.\n");

        } else if (strcmp(command_name, "CHANNELS") == 0) {
            printf("CHANNELS command entered.\n");

        } else if (strcmp(command_name, "UNSUB") == 0 && channel_id != -1) {
            printf("UNSUB command entered.\n");

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

        } else if (strcmp(command_name, "BYE") == 0 && channel_id == -1) {
            printf("BYE command entered.\n");

        } else {
            printf("Command entered is not valid.\n");
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