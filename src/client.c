#include <stdio.h> 
#include <signal.h>
#include "client.h"

// Variable to keep program running until SIGINT occurs
static volatile sig_atomic_t keep_running = 1;

int main(int argc, char ** argv) {
    // Setup the signal handling for SIGINT signal
    signal(SIGINT, handle_SIGINT);

    while (keep_running) {
        // Do the program stuff
    }
    
    printf("The program was successfully exited.\n");
    return 0;
}

void handle_SIGINT(int _) {
    (void)_; // To stop the compiler complaining
    printf("Handling the singal gracefully...\n");
    keep_running = 0;
}