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

/* Client command paring */
void decode_command(client_t *client, char *command, int *channel_id, char *message);

/* Setup a new client object after connection */
client_t* client_setup(int client_id);

/* Loop for each client process */
void client_processing (client_t *client);


// Action methods //

/* Subscribe CLIENT to CHANNEL_ID */
void subscribe(int channel_id, client_t *client);

/* List all channels CLIENT is connect to, and info about those channels */
void channels(client_t *client);

/* Unsubscribe CLIENT from CHANNEL */
void unsubscribe(int channel_id, client_t *client);

/* Show next unread message from any channel CLIENT is connected to */
void next(client_t *client);

/* Show next unread message from CHANNEL_ID if CLIENT is connected to it */
void nextChannel(int channel_id, client_t *client);

/* Send MSG to CHANNEL from CLIENT */
void sendMsg(int channel_id, client_t *client, char *message);

/* Disconnect CLIENT from the server */
int bye(client_t *client);


// Data structure methods //

/* Add message to the linked list */
msgnode_t* node_add(msgnode_t *head, msg_t *message);

/* Read message in specific channel and move the client read head */
msg_t* read_message(int channel_id, client_t *client);

/* Just retrieve next message in specific channel but do NOT move client read head */
msg_t* get_next_message(int channel_id, client_t *client);

/* Retrieve the number of unread messages */
int get_number_unread_messages(int channel_id, client_t *client);