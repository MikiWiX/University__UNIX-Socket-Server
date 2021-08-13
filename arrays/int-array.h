#ifndef INT_ARRAY_H_
#define INT_ARRAY_H_

typedef struct int_array {
    int *array;
    int high_index;
    int max_index;
} int_array;

int_array *create_int_array(int size);
int add_int(int_array *set, int num);
int remove_int(int_array *set, int index);
void drop_int_array(int_array *set);

#endif