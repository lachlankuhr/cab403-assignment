#include <data.h>

/* Subscribe CLIENT to CHANNEL_ID */
void subscribe(int channel_id, client_t *client, msgnode_t** messages);

/* List all channels CLIENT is connect to, and info about those channels */
void channels(client_t *client, msgnode_t** msg_list, int * messages_counts);

/* Unsubscribe CLIENT from CHANNEL */
void unsubscribe(int channel_id, client_t *client);

/* Show next unread message from any channel CLIENT is connected to */
void next(client_t *client, msgnode_t** msg_list);

/* Show next unread message from CHANNEL_ID if CLIENT is connected to it */
void nextChannel(int channel_id, client_t* client, msgnode_t** messages);

/* Send MSG to CHANNEL from CLIENT */
msgnode_t* sendMsg(int channel_id, msg_t* msg, client_t *client, msgnode_t** msg_list);

/* Disconnect CLIENT from the server */
int bye(client_t *client);

/* Sets the port to the one supplied otherwise defaults to 12345 */
int setServerPort(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

/* Starts the server to listen on the specified port number */
void startServer(int argc, char ** argv);

/* Read message in specific channel and move the client read head */
msg_t* read_message(int channel_id, client_t* client, msgnode_t** msg_list);

/* Just retrieve next message in specific channel but do NOT move client read head */
msg_t* get_next_message(int channel_id, client_t* client, msgnode_t** msg_list);

int get_number_unread_messages(int channel_id, client_t* client, msgnode_t** msg_list);

void decode_command(client_t* client, char* command, int * channel_id, char* message, int* true_neg_one);

client_t* client_setup(int client_id);

void client_processing (client_t* client);