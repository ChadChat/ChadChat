#include "hashmap.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Implementation of a hash table
 * By: Anex007
 */

hmap* hmap_init(size_t capacity, destroy_cb destroy)
{
    hmap* hm =  malloc(sizeof(hmap));
    
    hm->capacity = capacity;
    hm->list = malloc(sizeof(void*) * capacity);
    memset(hm->list, 0, capacity * sizeof(void*));

    hm->destroy = destroy;
    hm->size = 0;
    return hm;
}

bool hmap_exists(hmap* hm, const char* key)
{
    size_t h1 = hash(key) % hm->capacity;
    if(hm->list[h1] == NULL)
        return false;
    return true;
}

void hmap_get(hmap* hm, const char* key, void** data)
{
    size_t h1 = hash(key) % hm->capacity;
    *data = hm->list[h1];
}

void hmap_insert(hmap* hm, const char* key, void* data)
{
    size_t h1 = hash(key) % hm->capacity;
    hm->list[h1] = data;
    hm->size++;
}

void hmap_remove_element(hmap* hm, const char* key)
{
    size_t h1 = hash(key) % hm->capacity;
    hm->destroy(hm->list[h1]);
    hm->list[h1] = NULL;
}

void hmap_delete(hmap* hm)
{
    // add code to free memory based on the destroy cb function we have
    for(size_t i = 0; i < hm->capacity; i++)
        if(hm->list[i] != NULL)
            hm->destroy(hm->list[i]);
    
    free(hm->list);
    free(hm);
    memset(hm, 0, sizeof(hmap));
}

static long hash(const char* data)
{
    long x;
    const unsigned char* p = data;
    int len = strlen(data);

    x = *data << 7;
    while(--len >= 0)
        x = (1000003*x) ^ *p++;
    x ^= strlen(data);
    if(x == -1)
        return -2;
    return x;
}