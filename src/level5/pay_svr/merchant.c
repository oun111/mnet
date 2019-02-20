#include <stdlib.h>
#include <string.h>
#include "merchant.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"
#include "config.h"
#include "http_utils.h"



static int compare(char *s0, char *s1)
{
  return strcmp(s0,s1) ;
}

merchant_info_t get_merchant(merchant_entry_t entry, char *merchant_id)
{
  merchant_info_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,merchant_id,p,id,node,compare)) 
    return p ;

  return NULL ;
}

int 
save_merchant(merchant_entry_t entry, char *merchant_id, int sign_type, char *key, 
              char *priv_key, int param_type)
{
  merchant_info_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,merchant_id,p,id,node,compare)) {
    log_debug("merchant id %s already exists\n",merchant_id);
    return 0 ;
  }

  p = kmalloc(sizeof(struct merchant_info_s),0L);

  if (!p)
    return -1 ;


  strncpy(p->id,merchant_id,sizeof(p->id));

  p->sign_type = sign_type ;

  p->key = alloc_default_dbuffer();
  write_dbuf_str(p->key,key);

  p->priv_key = NULL ;
  if (p->sign_type==t_rsa && priv_key) {
    p->priv_key = alloc_default_dbuffer();
    write_dbuf_str(p->priv_key,priv_key);
  }

  p->param_type = param_type ;


  if (MY_RB_TREE_INSERT(&entry->u.root,p,id,node,compare)) {
    log_error("insert merchant id %s fail\n",merchant_id);
    kfree(p);
    return -1;
  }

  log_debug("adding merchant '%s'\n",p->id);

  entry->num_merchants ++;

  return 0;
}


static
int drop_merchant_internal(merchant_entry_t entry, merchant_info_t p)
{
  rb_erase(&p->node,&entry->u.root);

  drop_dbuffer(p->key);

  if (is_dbuffer_valid(p->priv_key))
    drop_dbuffer(p->priv_key);

  kfree(p);

  entry->num_merchants --;

  return 0;
}

int drop_merchant(merchant_entry_t entry, char *merchant_id)
{
  merchant_info_t p = get_merchant(entry,merchant_id);

  if (!p) {
    return -1;
  }

  drop_merchant_internal(entry,p);

  return 0;
}

int init_merchant_entry(merchant_entry_t entry, ssize_t pool_size)
{
  entry->u.root = RB_ROOT ;

  entry->num_merchants = 0L;

  log_debug("done!\n");

  return 0;
}

int release_all_merchants(merchant_entry_t entry)
{
  merchant_info_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_merchant_internal(entry,pos);
  }

  return 0;
}

size_t get_merchant_count(merchant_entry_t entry)
{
  return entry->num_merchants ;
}

int init_merchant_data(merchant_entry_t pm)
{
  tm_item_t pos,n;
  extern paySvr_config_t get_running_configs();
  tree_map_t mcfg = get_running_configs()->merchant_cfg;


  init_merchant_entry(pm,-1);

  rbtree_postorder_for_each_entry_safe(pos,n,&mcfg->u.root,node) {
    tree_map_t mch_map = pos->nest_map;
    char *mch_id = pos->key ;
    int stype = 0, ptype = 0;
    char *key = 0, *priv_key = 0, *ptr=0;

    key = get_tree_map_value(mch_map,"key");

    ptr = get_tree_map_value(mch_map,"sign_type");
    if (!strcmp(ptr,"md5")) {
      stype = t_md5;
    }
    else if (!strcmp(ptr,"rsa")) {
      stype = t_rsa ;
      priv_key = get_tree_map_value(mch_map,"priv_key");
    }

    ptr = get_tree_map_value(mch_map,"param_type");
    ptype = !strcmp(ptr,"json")?pt_json:pt_html ;

    save_merchant(pm,mch_id,stype,key,priv_key,ptype);
  }

  return 0;
}

