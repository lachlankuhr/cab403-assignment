#include <time.h>
#include <stdbool.h>

#define NUMCHANNELS 255

/* messages
 * Messages are stored in a hash table
 * Each bin in the hash table is a channel 
 */
typedef struct msg msg_t;
struct msg {
    char* string; // String of message being sent
    int user; // User who sent the message
    time_t time; // time message was sent - important for distinguishing between messages channels
};

/* node of hash table */
typedef struct msg_node msgnode_t;
struct msg_node {
    msg_t *msg;
    msgnode_t *next;
};

/* node of hash table */
typedef struct client_channel client_channel_t;
struct client_channel {
    int subscribed;
    int read;
};

typedef struct client client_t;
struct client {
    int id; // user's ID
    int socket; // socket user is connecting on
    client_channel_t channels[NUMCHANNELS]; // boolean to indicate if channel is subscribed to channel at index
    msgnode_t* read_msg[NUMCHANNELS]; // pointer to last message client has read in each channel.
    // read_msg is initialised when client joins a channel.
};

// Add message to the linked list
msgnode_t *node_add(msgnode_t *head, msg_t *message);
