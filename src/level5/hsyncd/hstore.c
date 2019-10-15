
#include <stdlib.h>
#include "mm_porting.h"
#include "hstore.h"
#include "log.h"
#include "formats.h"
#include "kernel.h"


static int sendh(hstore_object_t ph, common_format_t cf)
{
  const char *tbl = cf->tbl;
  const char *rowno = cf->row ;
  cf_pair_t pos;


  log_debug("sending table %s, row %s\n",tbl,rowno);

  list_for_each_entry(pos,&cf->cf_list,upper) {
    hclient_put(&ph->clt,tbl,rowno,pos->cf,pos->val);

    log_debug("cf: %s, val: %s\n",pos->cf,pos->val);

  }

  return 0;
}

static void* task(void *arg)
{
  common_format_t pos,n ;
  hstore_object_t ph = (hstore_object_t)arg ;


  // FIXME:
  while (1) {

    bool locked = false;

    if (pthread_mutex_trylock(&ph->task.lock))
      continue ;

    // fetch from local thread task list
    list_for_each_entry_safe(pos,n,&ph->task.lst,upper) {

      pthread_mutex_unlock(&ph->task.lock);

      // save to hbase
      sendh(ph,pos);

      if (pthread_mutex_trylock(&ph->task.lock)) {
        locked = false;
        break ;
      }

      locked = true ;
    }

    if (locked)
      pthread_mutex_unlock(&ph->task.lock);
  }

  return NULL;
}

int try_hstore(hstore_entry_t entry, common_format_t cf)
{
  hstore_object_t ph = list_first_entry(&entry->prio_list,struct hstore_object_s,pri_item);


  // pickup a task list to send
  {
    pthread_mutex_lock(&ph->task.lock);

    list_add(&cf->upper,&ph->task.lst);

    pthread_mutex_unlock(&ph->task.lock);
  }

  list_del(&ph->pri_item);
  list_add_tail(&ph->pri_item,&entry->prio_list);

  return 0;
}

int init_hstore_entry(hstore_entry_t entry, size_t count, const char *host, int port)
{
  entry->m_hObjs = kmalloc(sizeof(struct hstore_object_s)*count,0L);
  entry->obj_count = count ;

  // priority list
  INIT_LIST_HEAD(&entry->prio_list);

  for (int i=0;i<count;i++) {
    hstore_object_t ph = ((hstore_object_t)entry->m_hObjs)+i;
    hclient_t clt = &ph->clt ;

    // client connection to hbase
    if (hclient_init(clt,host,port)) {
      log_error("init hclient(server: %s:%d) fail!\n",host,port);
      return -1;
    }

    // task list
    INIT_LIST_HEAD(&ph->task.lst);

    pthread_mutex_init(&ph->task.lock,NULL);

    // thread
    pthread_create(&ph->worker,NULL,task,(void*)ph);

    // join priority list
    list_add(&ph->pri_item,&entry->prio_list);
  }

  return 0;
}

int release_hstore_entry(hstore_entry_t entry)
{
  for (int i=0;i<entry->obj_count;i++) {
    hstore_object_t ph = ((hstore_object_t)entry->m_hObjs)+i;
    hclient_t clt = &ph->clt ;

    pthread_join(ph->worker,NULL);
    pthread_cancel(ph->worker);

    hclient_release(clt);

    pthread_mutex_destroy(&ph->task.lock);

    // TODO: release the task list here ?
  }

  return 0;
}

