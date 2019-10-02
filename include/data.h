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
    time_t time_sent; // Time the message was sent
};

/* node of hash table */
typedef struct msg_node msgnode_t;
struct msg_node {
    char* key;
    msg_t *msg;
    msgnode_t *next;
};

/* message hash table - records messages in channel */
typedef struct msg_list msglist_t;
struct msg_list {
    msgnode_t **buckets;
    size_t size;
};

/* clients
 * Clients are also stored in a hash table, just as messages are
 * client object - stores ID and socket for connection 
 */
typedef struct client client_t;
struct client {
    int id; // user's ID
    int socket; // socket user is connecting on
    int channels[NUMCHANNELS]; // channels a client is connected to
    // Also an array of pointers so that the client knows where they are up to in the message history of each channel.
};

/* Node of hash table */
typedef struct client_node cnode_t;
struct client_node {
    char* key;
    client_t *client;
    cnode_t *next;
};

/* channel hash table - records client subscriptions 
 * (this may be poor way to do this and it might be better 
 * for each client to just store the channels they are subscribed to) 
 */
typedef struct client_list clist_t;
struct client_list {
    cnode_t **buckets;
    size_t size;
};

/* Hash table helper functions (basically straight from tut whoops)
 * Will move these if necessary, just put them here for now since they're helpers
 * for the hash tables 
 */
// Initialise message hash table
bool msglist_init(msglist_t *h, size_t n) {
    h->size = n;
    h->buckets = (msgnode_t **)calloc(n, sizeof(msgnode_t *));
    return h->buckets != 0;
}

// Initalise client hash table
bool clist_init(clist_t *h, size_t n) {
    h->size = n;
    h->buckets = (cnode_t **)calloc(n, sizeof(cnode_t *));
    return h->buckets != 0;
}

// hash function
size_t djb_hash(char *s) {
    size_t hash = 5381;
    int c;
    while ((c = *s++) != '\0') {
        // hash = hash * 33 + c
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// calculate offset for the bucket for key in hash table - msg
size_t msglist_index(msglist_t *h, char *key) {
    return djb_hash(key) % h->size;
}

// calculate offset for the bucket for key in hash table - client
size_t clist_index(clist_t *h, char *key) {
    return djb_hash(key) % h->size;
}

// Find pointer to head of list for key in hash table - msg.
msgnode_t * msglist_bucket(msglist_t *h, char *key) {
    return h->buckets[msglist_index(h, key)];
}
// Find pointer to head of list for key in hash table - client.
cnode_t * clist_bucket(clist_t *h, char *key) {
    return h->buckets[clist_index(h, key)];
}

// find item in msg table
msgnode_t * msglist_find(msglist_t *h, char *key) {
    for (msgnode_t *i = msglist_bucket(h, key); i != NULL; i = i->next) {
        if (strcmp(i->key, key) == 0) { // found the key
            return i;
        }
    }
    return NULL;
}

// find item in client table
cnode_t * clist_find(clist_t *h, char *key) {
    for (cnode_t *i = clist_bucket(h, key); i != NULL; i = i->next) {
        if (strcmp(i->key, key) == 0) { // found the key
            return i;
        }
    }
    return NULL;
}

// add msg to channel
bool msglist_add(msglist_t *h, char *key, msg_t* msg) {
    // allocate new item
    msgnode_t *newhead = (msgnode_t *)malloc(sizeof(msgnode_t));
    if (newhead == NULL) {
        return false;
    }
    newhead->key = key;
    newhead->msg = msg; // come back
    
    // hash key and place item in appropriate bucket
    size_t bucket = msglist_index(h, key);
    newhead->next = h->buckets[bucket];
    h->buckets[bucket] = newhead;
    return true;
}

// add client to channel
bool clist_add(clist_t *h, char *key, client_t* client) {
    // allocate new item
    cnode_t *newhead = (cnode_t *)malloc(sizeof(cnode_t));
    if (newhead == NULL) {
        return false;
    }
    newhead->key = key;
    newhead->client = client;
    
    // hash key and place item in appropriate bucket
    size_t bucket = clist_index(h, key);
    newhead->next = h->buckets[bucket];
    h->buckets[bucket] = newhead;
    return true;
}