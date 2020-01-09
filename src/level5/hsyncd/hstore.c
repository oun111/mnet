
#include <stdlib.h>
#include "mm_porting.h"
#include "formats.h"
#include "kernel.h"
#include "hstore.h"
#include "log.h"


static struct rpc_data_s g_rpcArgs ;


int do_hbase_store(hstore_entry_t entry, common_format_t cf)
{
  cf_pair_t pos;
  hstore_object_t ph = list_first_entry(&entry->prio_list, 
                       struct hstore_object_s,pri_item);


  strncpy(g_rpcArgs.table,cf->tbl,sizeof(g_rpcArgs.table));
  strncpy(g_rpcArgs.rowno,cf->row,sizeof(g_rpcArgs.rowno));

  list_for_each_entry(pos,&cf->cf_list,upper) {

    strncpy(g_rpcArgs.k,pos->cf,sizeof(g_rpcArgs.k));
    strncpy(g_rpcArgs.v,pos->val,sizeof(g_rpcArgs.v));

    rpc_clnt_tx(&ph->worker.clnt,&g_rpcArgs);
  }

  list_del(&ph->pri_item);
  list_add_tail(&ph->pri_item,&entry->prio_list);

  return 0;
}

int init_hstore_entry(hstore_entry_t entry, size_t count)
{
  entry->obj_count = count ;

  // priority list
  INIT_LIST_HEAD(&entry->prio_list);

  for (int i=0;i<entry->obj_count;i++) {
    hstore_object_t ph = kmalloc(sizeof(struct hstore_object_s),0L);

    // init rpc client
    rpc_clnt_init(&ph->worker.clnt,i);

    // join priority list
    list_add(&ph->pri_item,&entry->prio_list);
  }

  return 0;
}

int release_hstore_entry(hstore_entry_t entry)
{
  hstore_object_t pos,n ;


  list_for_each_entry_safe (pos,n,&entry->prio_list,pri_item) {
    rpc_clnt_release(&pos->worker.clnt);

    kfree(pos);
  }

  return 0;
}

