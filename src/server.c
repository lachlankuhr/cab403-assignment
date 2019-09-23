#include <stdio.h>
#include <stdlib.h>
#include <signal.h> // Signal handling
#include "server.h"

// Variable to keep program running until SIGINT occurs
static volatile sig_atomic_t keep_running = 1;

// Global variables
int port_number; 

int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handle_SIGINT);

    // Set the server port
    setServerPort(argc, argv); 

    while (keep_running) {
        // Do the program stuff
    }
    
    printf("The program was successfully exited.\n");
    // Do server stuff
    return 0;
}

void setServerPort(int argc, char ** argv) {
    if (argc > 1) {
        port_number = atoi(argv[1]);
    } else {
        port_number = 12345;
    }
    printf("The port number was set to: %i.\n", port_number);
}   

void handle_SIGINT(int _) {
    (void)_; // To stop the compiler complaining
    printf("Handling the singal gracefully...\n");
    keep_running = 0;
}