#ifndef LIST_IMP_CHAD
#define LIST_IMP_CHAD

typedef void (*destroy_cb)(void* data);
typedef int (*match_cb)(void* to_find, const void* item);
typedef void (*item_cb)(int index, const void* item, void* parameter);

typedef struct
{
    void** items;
    int capacity;
    int size;
    destroy_cb destroy;
}LIST;

#define LIST_SIZE(x) ((x)->size)
void LIST_destroy(LIST* list);
int LIST_find(LIST* list, void* to_find, match_cb match);
void LIST_cb(LIST* list, item_cb cb, void* parameter);
void* LIST_get(LIST* list, int index);
void LIST_remove(LIST* list, int index);
void LIST_set(LIST* list, int index, void* item);
bool LIST_insert(LIST* list, void* member);
LIST* LIST_new(int start_size, destroy_cb cb);

#endif /* ifndef LIST_IMP_CHAD */
