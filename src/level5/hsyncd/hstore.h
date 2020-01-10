#ifndef __HSTORE_H__
#define __HSTORE_H__


#include <pthread.h>
#include "hclient.h"
#include "list.h"
#include "rpc.h"



typedef struct hstore_entry_s* hstore_entry_t;

struct hstore_object_s {

  struct {
    struct rpc_clnt_s clnt ;
  } worker ;

  struct list_head pri_item ;
} ;
typedef struct hstore_object_s* hstore_object_t ;


struct hstore_entry_s {

  size_t obj_count ;

  struct list_head prio_list ;
} ;


extern int init_hstore_entry(hstore_entry_t entry, size_t count);

extern int release_hstore_entry(hstore_entry_t entry);

extern int do_hbase_store(hstore_entry_t entry, common_format_t cf);

#endif /* __HSTORE_H__*/
