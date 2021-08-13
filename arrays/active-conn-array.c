#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include "active-conn-array.h"

active_conn_set *create_conn(int size) {
    active_conn_set *set = malloc(sizeof(active_conn_set));
    active_conn_type *tab = malloc(sizeof(active_conn_type)*size);
    set->array = tab;
    set->high_index = -1;
    set->max_index = size-1;
    return set;
}

int add_conn(active_conn_set *set, int client_fd, struct sockaddr_in client_addr) {
    if (set->high_index >= set->max_index) {
        return 0;
    } else {
        active_conn_type new_connection = {client_fd, client_addr, time(NULL)};
        set->array[++set->high_index] = new_connection;
        return 1;
    }
}

int remove_conn(active_conn_set *set, int index) {
    if (set->high_index < index) {
        return 0;
    } else {
        set->array[index] = set->array[set->high_index];
        set->array[set->high_index--].client_fd = -1;
        return 1;
    }
}

void drop_conn_array(active_conn_set *set){
    free(set->array);
    free(set);
}