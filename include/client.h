/* Sets the client port number */
int setClientPort(int argc, char ** argv);

/* Starts the client on the specified port number */
void startClient(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

void decode_command(char* command, char * command_name, int * channel_id);

void *nextThreadFunc(void *channel);

<<<<<<< HEAD
void *livefeedThreadFunc(void *channel);
=======
void next(char * command);

void livefeed();

void decode_command(char* command, char * command_name, int * channel_id);

void livefeedChannel(int channel_id);

/* Deal with BYE or SIGINT */
void closeConnection();
>>>>>>> 60f61ced40b7e60bf51ff38222cf560ead5f721c
