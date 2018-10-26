#ifndef TASK_IMP_CHAD
#define TASK_IMP_CHAD

#include <stdlib.h>
#include <stdint.h> // Dont know if we need it or not.
#include <time.h>

#define TASK_RM_MULTIPLE 8

#define TASK_OPEN 0
#define TASK_OPEN_DMN 1
#define TASK_DONE 2
#define TASK_TERM 3

#define MAX_TASKS 8196

typedef struct {
   char task_state;
   time_t when;
   void* state;
   simple_cb cb_func
} IO_task;

// Add new task to the io.
int task_add_new(int time_off, void* state, simple_cb cb_func);
void task_change(int task_no, int time_off, void* state, simple_cb cb_func);
void task_delete(int task_no);

#endif /* ifndef TASK_IMP_CHAD */
