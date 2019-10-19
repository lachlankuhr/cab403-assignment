#include <time.h>
#include <stdbool.h>

#define NUMCHANNELS 255
#define MAXMESSAGELENGTH 1024

/* Messages
 * Messages are stored in a linked list
 * Each bin in the initial list is the head of a channel 
 */

/* Message information for within nodes */
typedef struct msg msg_t;
struct msg {
    char string[MAXMESSAGELENGTH]; // String of message being sent
    int user; // User who sent the message
    time_t time; // time message was sent - important for distinguishing between messages channels
};

/* Node of linked list */
typedef struct msg_node msgnode_t;
struct msg_node {
    msg_t *msg;
    msgnode_t *next;
};

/* Channel subscription information for within client array */
typedef struct client_channel client_channel_t;
struct client_channel {
    int subscribed;
    int read;
};

/* Client information data holder */
typedef struct client client_t;
struct client {
    int id; // user's ID
    int socket; // socket user is connecting on
    client_channel_t channels[NUMCHANNELS]; // boolean to indicate if channel is subscribed to channel at index
    msgnode_t *read_msg[NUMCHANNELS]; // pointer to last message client has read in each channel.
    // read_msg is initialised when client joins a channel.
};

typedef struct thread_args thread_args_t;
struct thread_args {
    int channel;
    int id;
};