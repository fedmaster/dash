#ifndef DART_TASKING_TASKLIST_H_
#define DART_TASKING_TASKLIST_H_

#include <dash/dart/base/macro.h>
#include <dash/dart/tasking/dart_tasking_priv.h>


void dart_tasking_tasklist_init() DART_INTERNAL;

/**
 * Prepend the task to the tasklist.
 */
void dart_tasking_tasklist_prepend(
  task_list_t           ** tl,
  struct dart_task_data *  task) DART_INTERNAL;

task_list_t * dart_tasking_tasklist_allocate_elem() DART_INTERNAL;

void dart_tasking_tasklist_deallocate_elem(task_list_t *tl) DART_INTERNAL;

void dart_tasking_tasklist_fini() DART_INTERNAL;

#endif /* DART_TASKING_TASKLIST_H_ */