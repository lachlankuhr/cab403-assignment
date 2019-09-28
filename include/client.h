/* Handles SIGINT signal to gracefully exit after crtl+c */
void handle_SIGINT();

/* Sets the client port number */
void setClientPort(int argc, char ** argv);

/* Starts the client on the specified port number */
void startClient(int argc, char ** argv);

/* Run the subscribe function - add specified channel to client's list of channels */
void subscribe(int channel_id);

/* List the channels and info about those channels */
void channels();

/* Unsubscribe from a channel */
void unsubscribe(int channel_id);

/* Get the next message from any of the channels */
void next();

/* Get the next message from the specified channel */
void next_channel(int channel_id);

/* Print a live feed of messages from all channels */
void livefeed();

/* Print a live feed of the specified channel */
void livefeed_channel(int channel_id);

/* Send a message to a channel */
void send_msg(int channel_id, char* msg);

/* Close the client program */
void bye();