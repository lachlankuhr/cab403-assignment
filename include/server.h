/* Receive a subscribe request from the server */
void subscribeServer();

/* Sets the port to the one supplied otherwise defaults to 12345 */
void setServerPort(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

/* Starts the server to listen on the specified port number */
void startServer(int argc, char ** argv);