// Client setup and close methods //

/* Starts the client on the specified port number */
void startClient(int argc, char **argv);

/* Sets the client port number */
int setClientPort(int argc, char **argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

/* Client command paring */
void decode_command(char* command, int *channel_id, int *next_neg_one);

/* Deal with BYE or SIGINT */
void closeConnection();


// Client threading methods //

/* Thread function for NEXT command */
void* nextThreadFunc(void *channel);

/* Thread function for LIVEFEED command */
void* livefeedThreadFunc(void *channel);

/* NEXT command issued and thread setup */
void next(char *command);

/* LIVEFEED command issued and thread setup */
void livefeed();

/* LIVEFEED command issued (with a channel ID) and thread setup */
void livefeedChannel(long channel_id);

/* Closes all active threads */
void closeThreads();
