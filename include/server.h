/* Sets the port to the one supplied otherwise defaults to 12345. */
void setServerPort(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handle_SIGINT();