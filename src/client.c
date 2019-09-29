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
#include "data.h"

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
        printf("Msg entered: %s\n", msg);
        

        // We may be able to compress this to a single send and have the 
        // server handle all the requests. We can look into this later.
        if (strcmp(command_name, "SUB") == 0 && channel_id != -1) {
            subscribe(channel_id);

        } else if (strcmp(command_name, "CHANNELS") == 0) {
            channels(subscribed_channels);

        } else if (strcmp(command_name, "UNSUB") == 0 && channel_id != -1) {
            unsubscribe(channel_id);

        } else if (strcmp(command_name, "NEXT") == 0 && channel_id != -1) {
            next_channel(channel_id);

        } else if (strcmp(command_name, "NEXT") == 0 && channel_id == -1) {
            next();

        } else if (strcmp(command_name, "LIVEFEED") == 0 && channel_id != -1) {
            livefeed_channel(channel_id);

        } else if (strcmp(command_name, "LIVEFEED") == 0 && channel_id == -1) {
            livefeed();

        } else if (strcmp(command_name, "SEND") == 0) {
            send_msg(channel_id, msg);
            printf("%s\n", msg);

        } else if (strcmp(command_name, "BYE") == 0 && channel_id == -1) {
            bye();

        } else {
            printf("Command entered is not valid.\n");
        }

    }

    close(sockfd);
    return 0;
}

// Subscribe to channel
void subscribe(int channel_id) {
    char command[MAXDATASIZE] = "SUB\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
    // TODO - Receive a message back from the server
}

// For now, just get this printing out the channels
void channels() {
    char command[MAXDATASIZE] = "CHANNELS\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    int channel_id = -1;
    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void unsubscribe(int channel_id) {
    char command[MAXDATASIZE] = "UNSUB\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void next() {
    char command[MAXDATASIZE] = "NEXT\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    int channel_id = -1;
    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void next_channel(int channel_id) {
    char command[MAXDATASIZE] = "NEXT\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void livefeed() {
    char command[MAXDATASIZE] = "LIVEFEED\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    int channel_id = -1;
    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void livefeed_channel(int channel_id) {
    char command[MAXDATASIZE] = "LIVEFEED\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void send_msg(int channel_id, char* msg) {
    char command[MAXDATASIZE] = "SEND\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
}

void bye() {
    char command[MAXDATASIZE] = "BYE\n";

    if (send(sockfd, command, MAXDATASIZE, 0) == -1) {
        perror("send");
    }

    int channel_id = -1;
    uint16_t channel_num_transfer = htons(channel_id);
    if (send(sockfd, &channel_num_transfer, sizeof(uint16_t), 0) == -1) {
        perror("send");
    }
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