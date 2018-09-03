#include "list.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

LIST* LIST_new(int start_size, destroy_cb cb)
{
   LIST* list = malloc(sizeof(LIST));
   if(!list)
       return NULL;
   list->capacity = start_size;
   list->size = 0;
   list->items = malloc(sizeof(void*) * start_size);
   list->destroy = cb;
   if(!list->items)
   {
      free(list);
      return NULL;
   }
   return list;
}

bool LIST_insert(LIST* list, void* member)
{
   if(list->size >= list->capacity)
   {
      list->capacity *= 2;
      list->items = reallocarray(list->items, list->capacity, sizeof(void*));
      if(!list->items)
          return false;
   }
   list->items[list->size++] = member;
   return true;
}

void LIST_set(LIST* list, int index, void* item)
{
   if(index >= 0 && index < LIST_SIZE(list))
   {
       if(list->destroy)
           list->destroy(list->items[index]);
       list->items[index] = item;
   }
}

void LIST_remove(LIST* list, int index)
{
   if(index < 0 || index >= LIST_SIZE(list))
       return;

   if(list->destroy)
      list->destroy(list->items[index]);

   list->items[index] = NULL;
   for (int i = index; i < LIST_SIZE(list)-1; i++) {
       v->items[i] = v->items[i+1];
   }
   list->items[list->size--] = NULL;
}

void* LIST_get(LIST* list, int index)
{
   if(index >= 0 && index < LIST_SIZE(list))
       return list->items[index];
   return NULL;
}

int LIST_find(LIST* list, void* to_find, match_cb match)
{
   for (int i = 0; i < LIST_SIZE(list); i++)
       if(match(to_find, list->items[i]))
           return i;
   return -1;
}

void LIST_cb(LIST* list, item_cb cb, void* parameter)
{
   for (int i = 0; i < LIST_SIZE(list); i++)
      cb(i, list->items[i], parameter);
}

void LIST_destroy(LIST* list)
{
   if(list->destroy)
      for (int i = 0; i < LIST_SIZE(list); ++i)
         list->destroy(list->items[i]);

   free(list->items);
   free(list);
   memset(list, 0, sizeof(LIST));
}
