#include <stdio.h>
#include <stdlib.h>
#include "server.h"

int port; 

int main(int argc, char ** argv) {
    setServerPort(argc, argv); 
    // Do server stuff
    return 0;
}

void setServerPort(int argc, char ** argv) {
    if (argc > 1) {
        port = atoi(argv[1]);
    } else {
        port = 12345;
    }
}   