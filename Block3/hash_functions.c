//
// Created by Daniel on 14.12.2020.
//
#include <stdio.h>
#include "uthash.h"

typedef struct hash_item{
    const char* key;
    const void* value;
    unsigned int value_length;
    UT_hash_handle hh;
} hash_item;

hash_item* new_hash(){
    return NULL;
}

int set_item(hash_item** hash, char* key, void* value, unsigned int value_length) {
    hash_item* new_item;
    HASH_FIND_STR(*hash, key, new_item);
    if (new_item != NULL)
        return -1;

    new_item = malloc(sizeof(hash_item));
    new_item->key = calloc(strlen(key) + 1, sizeof(char));
    new_item->value = malloc(value_length);
    memcpy(new_item->key, key, strlen(key) * sizeof(char));
    memcpy(new_item->value, value, value_length);
    new_item->value_length = value_length;
    HASH_ADD_KEYPTR(hh, *hash, new_item->key, strlen(new_item->key), new_item);
    return 0;
}

int get_item(hash_item** hash, char* key, void** value, unsigned int* value_length){
    hash_item* item;
    HASH_FIND_STR(*hash, key, item);
    if (item == NULL)
        return -1;
    *value = malloc(item->value_length);
    memcpy(*value, item->value, item->value_length);
    *value_length = item->value_length;
    return 0;

}

int delete_item(hash_item** hash, char* key){
    hash_item* tbd;
    HASH_FIND_STR(*hash, key, tbd);
    if (tbd == NULL)
        return -1;

    HASH_DEL(*hash, tbd);
    free(tbd->key);
    free(tbd->value);
    free(tbd);
    return 0;

}

int free_hash(hash_item** hash){
    if (*hash == NULL)
        return -1;

    hash_item* tbd, *tmp;
    HASH_ITER(hh, *hash, tbd, tmp) {
        HASH_DEL(*hash, tbd);
        free(tbd->key);
        free(tbd->value);
        free(tbd);
    }
    return 0;

}