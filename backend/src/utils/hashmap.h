#ifndef HMAP_IMP_CHAD
#define HMAP_IMP_CHAD

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef void (*destroy_cb)(void* data);

typedef struct
{
    size_t capacity;
    destroy_cb destroy;
    uint32_t size;
    void** list;
}hmap;

hmap* hmap_init(size_t capacity, destroy_cb destroy);
bool hmap_exists(hmap* hm, const char* key);
void hmap_get(hmap* hm, const char* key, void** data);
void hmap_insert(hmap* hm, const char* key, void* data);
void hmap_remove_element(hmap* hm, const char* key);
void hmap_delete(hmap* hm);
static long hash(const char* data);

#endif