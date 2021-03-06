#include <data.h>

// Setup methods //

/* Starts the server to listen on the specified port number */
void startServer(int argc, char **argv);

/* Sets the port to the one supplied otherwise defaults to 12345 */
int setServerPort(int argc, char **argv);

/* Sets up shared memory addresses for message data */
void setupSharedMem();

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();
void handleChildSIGINT(int _);
void childClose();

/* Handles closing zombie processes */
void handleSIGCHLD(int signum); 

/* Client command paring */
void decode_command(client_t *client, char *command, long *channel_id, char *message);

/* Setup a new client object after connection */
void client_setup(client_t *client, int client_id);

/* Loop for each client process */
void client_processing(client_t *client, pid_t current_process);


// Action methods //

/* Subscribe CLIENT to CHANNEL_ID */
void subscribe(long channel_id, client_t *client);

/* List all channels CLIENT is connect to, and info about those channels */
void channels(client_t *client);

/* Unsubscribe CLIENT from CHANNEL */
void unsubscribe(long channel_id, client_t *client);

/* Show next unread message from any channel CLIENT is connected to */
void next(client_t *client);

/* Show next unread message from CHANNEL_ID if CLIENT is connected to it */
void nextChannel(long channel_id, client_t *client);

/* Send MSG to CHANNEL from CLIENT */
void sendMsg(long channel_id, client_t *client, char *message);

/* Disconnect CLIENT from the server */
int bye(client_t *client);


// Data structure use and storage methods //

/* Add message to the linked list */
msgnode_t* node_add(msgnode_t *head, msg_t *message);

/* Read message in specific channel and move the client read head */
msg_t* read_message(long channel_id, client_t *client);

/* Just retrieve next message in specific channel but do NOT move client read head */
msg_t* get_next_message(long channel_id, client_t *client);

/* Retrieve the number of unread messages */
int get_number_unread_messages(long channel_id, client_t *client);