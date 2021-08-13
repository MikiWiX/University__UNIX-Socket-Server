#include <stdlib.h>

#include "int-array.h"

int_array *create_int_array(int size) {
    int_array *set = malloc(sizeof(int_array));
    int *tab = malloc(sizeof(int)*size);
    set->array = tab;
    set->high_index = -1;
    set->max_index = size-1;
    return set;
}

int add_int(int_array *set, int num) {
    if (set->high_index >= set->max_index) {
        return 0;
    } else {
        set->array[++set->high_index] = num;
        return 1;
    }
}

int remove_int(int_array *set, int index) {
    if (set->high_index < index) {
        return 0;
    } else {
        set->array[index] = set->array[set->high_index];
        set->array[set->high_index--] = 0;
        return 1;
    }
}

void drop_int_array(int_array *set){
    free(set->array);
    free(set);
}