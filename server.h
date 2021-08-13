#ifndef SERVER_h_
#define SERVER_h_

// max message size, works on both input and output; it is set in server.c
const int MAX_MESSAGE_SIZE;

/*
* add message to send
*/
void new_message(int client_fd, char *buf, int size);

/*
* erase all pending messages to a given client_fd and close the connection now
*/
void force_disconnect(int client_fd);

/*
* try to send all pending messages to that client_fd before disconnecting
*/
void disconnect(int client_fd);

#endif