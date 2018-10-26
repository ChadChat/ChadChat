#include "task.h"

IO_task* all_tasks;
int num_tasks = 0;

// only free and transverse the array if:
//    Every element after the task is either done or terminated, or daemonized


// make this memory management work!!
void task_init(void) {
   all_tasks = calloc(MAX_TASKS, sizeof(IO_task*));
}

int add_new_task(int time_off, void* state, simple_cb cb_func char daemon) {
   if(num_tasks >= MAX_TASKS)
       return -1;
   all_tasks[num_tasks] = malloc(sizeof(IO_task));
   all_tasks[num_tasks].when = time(NULL) + time_off;
   all_tasks[num_tasks].state = state;
   all_tasks[num_tasks].cb_func = cb_func;
   if (daemon)
      all_tasks[num_tasks].task_state = TASK_OPEN_DMN;
   else
      all_tasks[num_tasks].task_state = TASK_OPEN;
   return num_tasks++;
}

// changes the task struct.
void change_task(int task_no, int time_off, void* state, simple_cb cb_func) {
   if (time_off != 0)
       all_tasks[task_no].when = time(NULL) + time_off;
   if (state != NULL)
       all_tasks[task_no].state = state;
   if (cb_func != NULL)
       all_tasks[task_no].cb_func = cb_func;
}

// simple integer compare function for a one time use.
static int compare_ints(const void* _i1, const void* _i2)
{
    int i1 = *((int*)_i1);
    int i2 = *((int*)_i2);
    return (i1 > i2) - (i1 < i2);
}

// deletes the task (I optimized it).
void delete_task(int task_no) {
   static char tasks_to_remove = 0;
   static int tasks_indexs[TASK_RM_MULTIPLE];

   all_tasks[task_no].when = 0;  // I made an if statement so that the loop ignores if the time is 0.
   tasks_indexs[tasks_to_remove] = task_no;
   tasks_to_remove++;

   // speed up the process of only freeing and rearraging if it's 5.
   if(tasks_to_remove == TASK_RM_MULTIPLE) {
      int tmp;
      // This optimizes the removing
      qsort(tasks_index, TASK_RM_MULTIPLE, sizeof(int), compare_ints);
      // Code to remove actual elements here.
      // Reverse the list so that you can transverse the list faster.
      for (int i = TASK_RM_MULTIPLE-1; i >= 0; i--) {
          free(tasks_indexs[i]);  // free the  actual memory.
          // Shift every element after the list here.
          for (int j = tasks_indexs[i]; j < num_tasks; j++) {
              
          }
      }

      tasks_to_remove = 0;
   }
}

void task_destroy(void) {
   for (int i = 0; i < num_tasks; i++) {
   }
   free(all_tasks);
}

