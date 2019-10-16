#ifndef __HSTORE_H__
#define __HSTORE_H__


#include <pthread.h>
#include "hclient.h"
#include "list.h"



typedef struct hstore_entry_s* hstore_entry_t;

struct hstore_object_s {
  pthread_t worker ;

  struct hbase_client_s clt ;

  struct {
    pthread_mutex_t lock ;
    pthread_cond_t cond ;
  } work_signal ;

  struct {
    struct list_head lst ;

    pthread_mutex_t lock ;
  } task;

  hstore_entry_t parent ;

  struct list_head pri_item ;
} ;
typedef struct hstore_object_s* hstore_object_t ;


struct hstore_entry_s {

  hstore_object_t m_hObjs ;

  size_t obj_count ;

  struct {
    char host[32];
    int port ;
  } hbase ;

  struct list_head prio_list ;
} ;


extern int init_hstore_entry(hstore_entry_t entry, size_t count, 
                             const char *host, int port);

extern int release_hstore_entry(hstore_entry_t entry);

extern int do_hbase_store(hstore_entry_t entry, common_format_t cf);

extern void hstore_start_work(hstore_entry_t entry);

#endif /* __HSTORE_H__*/
