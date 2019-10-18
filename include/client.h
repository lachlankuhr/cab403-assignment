/* Starts the client on the specified port number */
void startClient(int argc, char **argv);

/* Sets the client port number */
int setClientPort(int argc, char **argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

/*  */
void decode_command(char* command, int *channel_id, int *next_neg_one);

/*  */
void *nextThreadFunc(void *channel);

/*  */
void *livefeedThreadFunc(void *channel);

/*  */
void next(char * command);

/*  */
void livefeed();

/*  */
void livefeedChannel(int channel_id);

/*  */
void closeThreads();

/* Deal with BYE or SIGINT */
void closeConnection();