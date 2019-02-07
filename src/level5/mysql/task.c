#include "task.h"
#include "kernel.h"
#include "log.h"


int init_task_pool(task_entry_t entry, size_t pool_size)
{
  task_t pos,n ;


  INIT_LIST_HEAD(&entry->taskq);

  entry->pool = create_obj_pool("mp2-task-pool",pool_size,struct task_s);

  list_for_each_objPool_item(pos,n,entry->pool) {
    pos->backend_numbers = alloc_default_dbuffer();
    pos->req = alloc_default_dbuffer();
  }

  return 0;
}

void release_task_pool(task_entry_t entry)
{
  task_t pos,n ;


  list_for_each_objPool_item(pos,n,entry->pool) {
    drop_dbuffer(pos->backend_numbers);
    drop_dbuffer(pos->req);
  }

  release_obj_pool(entry->pool,struct task_s);
}

task_t alloc_task(task_entry_t entry, int backend_count)
{
  task_t ptask = obj_pool_alloc(entry->pool,struct task_s);


  if (!ptask) {
    ptask = obj_pool_alloc_slow(entry->pool,struct task_s);
    if (ptask) {
      ptask->backend_numbers = alloc_default_dbuffer();
      ptask->req = alloc_default_dbuffer();
    }
  }

  if (!ptask) {
    log_error("fatal: cant allocate more tasks\n");
    return NULL ;
  }

  if (backend_count<1) {
    log_error("fatal: invalid backend count '%d'\n",backend_count);
    return NULL ;
  }

  ptask->is_use = 1;

  ptask->backend_count = backend_count;

  //ptask->total_eof = 0;
  ptask->ptn_pos = -1;

  ptask->backend_numbers = realloc_dbuffer(ptask->backend_numbers, 
                                           sizeof(int)*backend_count);

  ptask->ref_count = 0;

  ptask->sn = 0;

  INIT_LIST_HEAD(&ptask->taskq_item);

  return ptask ;
}

int free_task(task_entry_t entry, task_t ptask)
{
  ptask->is_use = 0;

  obj_pool_free(entry->pool,ptask);

  return 0;
}

void show_pool_item_count(task_entry_t entry)
{
  task_t pos;
  int cnt=0;

  list_for_each_entry(pos,&entry->pool->free_pool,pool_item) {
    cnt++;
  }
  log_debug("items freed: %d\n",cnt);
}

void save_task_req(task_t ptask, char *inb, size_t sz_in)
{
  ptask->req = write_dbuffer(ptask->req,inb,sz_in);
}

int get_task_req(task_t ptask, char **inb, size_t *sz_in)
{
  *inb = ptask->req ;
  *sz_in = dbuffer_data_size(ptask->req);

  return 0;
}

void save_task_bk_num(task_t ptask, int pos, int no)
{
  if (pos>=0 && pos<ptask->backend_count)
    ptask->backend_numbers[pos] = no ;
}

int get_task_bk_num(task_t ptask, int pos)
{
  if (pos>=0 && pos<ptask->backend_count)
    return ptask->backend_numbers[pos] ;

  return -1;
}

void task_ref_set(task_t ptask, int val)
{
  ptask->ref_count = val ;
}

void task_ref_add(task_t ptask)
{
  ptask->ref_count++ ;
}

void task_ref_sub(task_t ptask)
{
  ptask->ref_count-- ;
}

int get_task_ref(task_t ptask)
{
  return ptask->ref_count ;
}

void task_enqueue(task_entry_t entry, task_t tsk)
{
  list_add_tail(&tsk->taskq_item,&entry->taskq);
}

task_t task_dequeue(task_entry_t entry)
{
  task_t tsk = 0;


  if (list_empty(&entry->taskq)) {
    return NULL;
  }

  tsk = list_first_entry_or_null(&entry->taskq,struct task_s,taskq_item);
  if (!tsk) {
    log_error("fatal: null task in taskq\n");
    return NULL;
  }

  list_del(&tsk->taskq_item);

  return tsk;
}

