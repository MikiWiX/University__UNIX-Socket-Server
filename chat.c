#include <netinet/in.h>

#include "server.h"
#include "chat.h"

/*
* This function is being called every time server received a message
* @arg client_fd: file descriptor used by client
* @arg client_addr: client adress
* @arg *message: pointer to a buffer with received message
* @arg size: message lenght
*/
void respond(int client_fd, struct sockaddr_in client_addr, char *message, int size){
    new_message(client_fd, message, size);
    disconnect(client_fd);
}

/*
* This function is being called every time server disconnects from a user
* @arg client_fd: file descriptor used by client
* @arg client_addr: client adress
*/
void client_disconnected(int client_fd, struct sockaddr_in client_addr){

}