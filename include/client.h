/* Sets the client port number */
void setClientPort(int argc, char ** argv);

/* Starts the client on the specified port number */
void startClient(int argc, char ** argv);

/* Sets the port to the one supplied otherwise defaults to 12345 */
void setClientPort(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();