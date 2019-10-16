
#include <stdlib.h>
#include "mm_porting.h"
#include "formats.h"
#include "hstore.h"
#include "log.h"
#include "kernel.h"



static void send_work_signal(hstore_object_t ph)
{
  pthread_cond_signal(&ph->work_signal.cond);
}

static void wait_to_work(hstore_object_t ph)
{
  pthread_cond_wait(&ph->work_signal.cond,&ph->work_signal.lock);
}

static int sendh(hstore_object_t ph, common_format_t cf)
{
  hstore_entry_t entry = ph->parent ;
  const char *tbl = cf->tbl;
  const char *rowno = cf->row ;
  cf_pair_t pos;
  int rc = 0;


  log_debug("sending table %s, row %s\n",tbl,rowno);

  list_for_each_entry(pos,&cf->cf_list,upper) {

    do {
      rc = 0;
      // put fail
      if (hclient_put(&ph->clt,tbl,rowno,pos->cf,pos->val)) {
        log_debug("retrying put...\n");
        hclient_release(&ph->clt);
        hclient_init(&ph->clt,entry->hbase.host,entry->hbase.port);
        rc = -1;
      }
    } while (rc<0) ;

    log_debug("cf: %s, val: %s\n",pos->cf,pos->val);
  }

  return rc;
}

static void* task(void *arg)
{
  common_format_t pos,n ;
  hstore_object_t ph = (hstore_object_t)arg ;


  // FIXME:
  while (1) {

    int rc = 0;
    bool locked = false;


    wait_to_work(ph);

    if (pthread_mutex_trylock(&ph->task.lock)) {
      continue ;
    } else 
      locked = true ;

    // fetch from local thread task list
    list_for_each_entry_safe(pos,n,&ph->task.lst,upper) {

      pthread_mutex_unlock(&ph->task.lock);

      // save to hbase
      rc = sendh(ph,pos);

      if (pthread_mutex_trylock(&ph->task.lock)) {
        locked = false;
        break ;
      }

      // save to hbase ok, remove th cf
      //(void)rc;
      if (!rc)
      {
        list_del(&pos->upper);
        free_common_format(pos);
      }

      locked = true ;
    }

    if (locked)
      pthread_mutex_unlock(&ph->task.lock);
  }

  return NULL;
}

int do_hbase_store(hstore_entry_t entry, common_format_t cf)
{
  hstore_object_t ph = list_first_entry(&entry->prio_list,struct hstore_object_s,
                                        pri_item);


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

void hstore_start_work(hstore_entry_t entry)
{
  hstore_object_t pos;

  list_for_each_entry(pos,&entry->prio_list,pri_item) {
    send_work_signal(pos);
  }
}

int init_hstore_entry(hstore_entry_t entry, size_t count, const char *host, int port)
{
  log_debug("force worker count to 1\n");

  entry->obj_count = /*count*/1 ;
  entry->m_hObjs = kmalloc(sizeof(struct hstore_object_s)*entry->obj_count,0L);

  strncpy(entry->hbase.host,host,sizeof(entry->hbase.host));
  entry->hbase.port = port ;

  // priority list
  INIT_LIST_HEAD(&entry->prio_list);

  for (int i=0;i<entry->obj_count;i++) {
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

    // the lock indicating start working
    pthread_mutex_init(&ph->work_signal.lock,NULL);
    pthread_cond_init(&ph->work_signal.cond,NULL);

    ph->parent = entry ;

    // join priority list
    list_add(&ph->pri_item,&entry->prio_list);

    // thread
    pthread_create(&ph->worker,NULL,task,(void*)ph);
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

    pthread_cond_destroy(&ph->work_signal.cond);
    pthread_mutex_destroy(&ph->work_signal.lock);

    // TODO: release the task list here ?
  }

  return 0;
}

