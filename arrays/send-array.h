#ifndef SEND_ARRAY_H_
#define SEND_ARRAY_H_

#include <time.h>

// if you wanna change supported type, just change this one typedef value
//structure with all details needed for writing to client_fd
typedef struct to_send_type {
    int client_fd;
    char *buf;
    int size;
    time_t init_time;
}to_send_type;

typedef struct to_send_set {
    to_send_type *array;
    int high_index;
    int max_index;
}to_send_set;

to_send_set *create_to_send(int size);
int add_to_send(to_send_set *set, int client_fd, char *buf, int size);
int remove_to_send(to_send_set *set, int index);
void drop_to_send_array(to_send_set *set);

#endif