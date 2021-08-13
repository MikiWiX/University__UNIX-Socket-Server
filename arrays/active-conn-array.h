#ifndef ACTIVE_CONN_ARRAY_H_
#define ACTIVE_CONN_ARRAY_H_

#include <netinet/in.h>

// if you wanna change supported type, just change this one typedef value
//structure with all details needed to keep track of connections
typedef struct active_conn_type {
    int client_fd;
    struct sockaddr_in client_addr;
    time_t lastly_used;
}active_conn_type;

typedef struct active_conn_set {
    active_conn_type *array;
    int high_index;
    int max_index;
} active_conn_set;

active_conn_set *create_conn(int size);
int add_conn(active_conn_set *set, int client_fd, struct sockaddr_in client_addr);
int remove_conn(active_conn_set *set, int index);
void drop_conn_array(active_conn_set *set);

#endif