/* Handles SIGINT signal to gracefully exit after crtl+c */
void handle_SIGINT();

/* Sets the client port number */
void setClientPort(int argc, char ** argv);

/* Starts the client on the specified port number */
void startClient(int argc, char ** argv);