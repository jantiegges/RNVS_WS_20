//
// Created by Daniel on 14.12.2020.
//

#ifndef BLOCK3_HASH_FUNCTIONS_H
#define BLOCK3_HASH_FUNCTIONS_H

typedef struct hash_item hash_item;

hash_item* new_hash();
int set_item(hash_item** hash, void* key, void* value, unsigned int key_length, unsigned int value_length);
int get_item(hash_item** hash, void* key, unsigned int key_length, void** value, unsigned int* value_length);
int delete_item(hash_item** hash, void* key, unsigned int key_length);
int free_hash(hash_item** hash);
#endif //BLOCK3_HASH_FUNCTIONS_H
