#ifndef CHAT_H_
#define CHAT_H_

#include <netinet/in.h>

void respond(int client_fd, struct sockaddr_in client_addr, char *message, int size);
void client_disconnected(int client_fd, struct sockaddr_in client_addr);

#endif