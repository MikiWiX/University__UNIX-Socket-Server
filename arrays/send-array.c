#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "send-array.h"

to_send_set *create_to_send(int size) {
    to_send_set *set = malloc(sizeof(to_send_set));
    to_send_type *tab = malloc(sizeof(to_send_type)*size);
    set->array = tab;
    set->high_index = -1;
    set->max_index = size-1;
    return set;
}

int add_to_send(to_send_set *set, int client_fd, char *buf, int size) {
    if (set->high_index >= set->max_index) {
        return 0;
    } else {
        char *new_buf = (char*) malloc(size);
        strcpy(new_buf, buf);
        to_send_type message = {client_fd, new_buf, size, time(NULL)};
        set->array[++set->high_index] = message;
        return 1;
    }
}

int remove_to_send(to_send_set *set, int index) {
    if (set->high_index < index) {
        return 0;
    } else {
        free(set->array[index].buf);
        set->array[index] = set->array[set->high_index];
        set->array[set->high_index--].buf = NULL;
        return 1;
    }
}


void drop_to_send_array(to_send_set *set){
    for(int i=0; i<=set->high_index; i++){
        free(set->array[i].buf);
    }
    free(set->array);
    free(set);
}