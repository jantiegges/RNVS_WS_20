//
// Created by Daniel on 14.12.2020.
//
#include <stdio.h>
#include "uthash.h"

#define ACK 0b1000

typedef struct hash_item{
    void* key;
    void* value;
    unsigned int key_length;
    unsigned int value_length;
    UT_hash_handle hh;
} hash_item;

hash_item* new_hash(){
    return NULL;
}

int set_item(hash_item** hash, void* key, void* value, unsigned int key_length, unsigned int value_length) {
    hash_item* new_item;
    HASH_FIND(hh, *hash, key, key_length, new_item);
    if (new_item) {
        HASH_DEL(*hash, new_item);
        free(new_item->key);
        free(new_item->value);
        free(new_item);
    }

    new_item = malloc(sizeof(hash_item));
    new_item->key = malloc(key_length);
    new_item->value = malloc(value_length);
    memcpy(new_item->key, key, key_length);
    memcpy(new_item->value, value, value_length);
    new_item->key_length = key_length;
    new_item->value_length = value_length;
    HASH_ADD_KEYPTR(hh, *hash, new_item->key, new_item->key_length, new_item);
    return ACK;
}

int get_item(hash_item** hash, void* key, unsigned int key_length, void** value, unsigned int* value_length){
    hash_item* item;
    HASH_FIND(hh, *hash, key, key_length, item);
    if (item == NULL)
        return 0;
    *value = malloc(item->value_length);
    memcpy(*value, item->value, item->value_length);
    *value_length = item->value_length;
    return ACK;

}

int delete_item(hash_item** hash, void* key, unsigned int key_length){
    hash_item* tbd;
    HASH_FIND(hh, *hash, key, key_length, tbd);
    if (tbd == NULL)
        return 0;

    HASH_DEL(*hash, tbd);
    free(tbd->key);
    free(tbd->value);
    free(tbd);
    return ACK;

}

int free_hash(hash_item** hash){
    if (*hash == NULL)
        return 0;

    hash_item* tbd, *tmp;
    HASH_ITER(hh, *hash, tbd, tmp) {
        HASH_DEL(*hash, tbd);
        free(tbd->key);
        free(tbd->value);
        free(tbd);
    }
    return ACK;

}