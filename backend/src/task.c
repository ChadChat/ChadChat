#include "task.h"

IO_task* all_tasks;
int num_tasks = 0;


void task_init(void)
{
   &all_tasks = calloc(MAX_TASKS, sizeof(IO_task*));
}

int task_add_new(int time_off, void* state, simple_cb cb_func, char daemon)
{
   int task_no;
   // see if there's any done tasks in the list.
   if ((task_no = task_free()) != -1) {
       all_tasks[task_no].when = time(NULL) + time_off;
       all_tasks[task_no].state = state;
       all_tasks[task_no].cb_func = cb_func;
       all_tasks[task_no].when = TASK_OPEN;
       return task_no;
   }

   if (num_tasks >= MAX_TASKS)
       return -1;

   all_tasks[num_tasks] = malloc(sizeof(IO_task));
   all_tasks[num_tasks].when = time(NULL) + time_off;
   all_tasks[num_tasks].state = state;
   all_tasks[num_tasks].cb_func = cb_func;
   all_tasks[num_tasks].task_state = TASK_OPEN;
   return num_tasks++;
}

static int task_free(void)
{
   for (int i = 0; i < num_tasks; i++) {
       // return the task_number if either the task is done or terminated.
       if (all_tasks[i].task_state == TASK_DONE)
           return i;
   }
   return -1;
}

// changes the task struct.
void task_change(int task_no, int time_off, void* state, simple_cb cb_func)
{
   // only change the task stuff if it's valid.
   if (all_tasks[task_no] != TASK_OPEN)
      return;

   if (time_off != 0)
       all_tasks[task_no].when = time(NULL) + time_off;
   if (state != NULL)
       all_tasks[task_no].state = state;
   if (cb_func != NULL)
       all_tasks[task_no].cb_func = cb_func;
}

// deletes the task (I optimized it).
void task_delete(int task_no)
{
   static int tasks_to_remove = 0;

   // only if it's valid.
   if (all_tasks[task_no].task_state != TASK_OPEN)
       return;

   all_tasks[task_no].task_state = TASK_DONE;
   tasks_to_remove++;

   // speed up the process of only freeing if the removed tasks >= TASK_RM_MULTPLE.
   // First it scans if there are any done tasks in reverse, if there are remove it in reverse order.
   if (tasks_to_remove >= TASK_RM_MULTIPLE) {
       int l_index = -1;
       for (int i = num_tasks-1; i >= 0; i--) {
           if (all_tasks[i].task_state == TASK_DONE)
               l_index = i;
       }

       // If there's nothing to remove return
       if (l_index == -1)
           return;

       for (int i = l_index; i < num_tasks; i++) {
           free(all_tasks[i]);
       }
       // update num_tasks and tasks_to_remove.
       num_tasks = l_index;
       tasks_to_remove = tasks_to_remove - (num_tasks - l_index);
   }
}

void task_destroy(void)
{
   for (int i = 0; i < num_tasks; i++) {
       free(all_tasks[i]);
   }
   free(all_tasks);
}
