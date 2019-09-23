#include <stdio.h> 
#include <stdlib.h>
#include <netdb.h> 
#include "client.h"

// Global variables
int port_number; 
struct hostent *he;
struct sockaddr_in server_addr; /* connector's address information */

int main(int argc, char ** argv) {
    // ONLY JUST STARTED THIS IMPLEMENTATION. FEEL FREE TO OVERRIDE.
    if (argc != 3) {
        printf("Please enter both a hostname and port number to connect.\n");
        return -1;
    } else {
        port_number = atoi(argv[2]);
    }
    if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */
		herror("gethostbyname");
		exit(1);
	}
    return 0;
}