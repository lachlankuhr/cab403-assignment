/* Sets the client port number */
int setClientPort(int argc, char ** argv);

/* Starts the client on the specified port number */
void startClient(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

void decode_command(char* command, char * command_name, int * channel_id);

void *nextThreadFunc(void *channel);

void *livefeedThreadFunc(void *channel);