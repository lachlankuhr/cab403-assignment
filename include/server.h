#include <data.h>

/* Subscribe CLIENT to CHANNEL_ID */
void subscribe(int channel_id, client_t *client);

/* List all channels CLIENT is connect to, and info about those channels */
void channels(client_t *client);

/* Unsubscribe CLIENT from CHANNEL */
void unsubscribe(int channel_id, client_t *client);

/* Show next unread message from any channel CLIENT is connected to */
void next(client_t *client);

/* Show next unread message from CHANNEL_ID if CLIENT is connected to it */
void nextChannel(int channel_id, client_t* client);

/* Show all unread messages in all channels CLIENT is connected to, and continue to show new unread messages */
void livefeed(client_t *client);

/* Show all unread messages from CHANNEL_ID if CLIENT is connected to it, and continue to show messages as they come through */
void livefeedChannel(int channel_id, client_t *client);

/* Send MSG to CHANNEL from CLIENT */
void sendMsg();

/* Disconnect CLIENT from the server */
void bye(client_t *client);

/* Sets the port to the one supplied otherwise defaults to 12345 */
void setServerPort(int argc, char ** argv);

/* Handles SIGINT signal to gracefully exit after crtl+c */
void handleSIGINT();

/* Starts the server to listen on the specified port number */
void startServer(int argc, char ** argv);