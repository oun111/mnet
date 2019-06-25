#include <stdlib.h>
#include <string.h>
#include "tree_map.h"
#include "merchant.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"
#include "config.h"
#include "http_utils.h"
#include "md.h"
#include "pay_data.h"



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
save_merchant(merchant_entry_t entry, char *merchant_id, tree_map_t mch_info)
{
  char *pv = 0;
  merchant_info_t p = 0;
  extern pay_channels_entry_t get_pay_channels_entry() ;
  pay_channels_entry_t pe = get_pay_channels_entry() ;


  if (!MY_RB_TREE_FIND(&entry->u.root,merchant_id,p,id,node,compare)) {
    log_debug("merchant id %s already exists\n",merchant_id);
    return 0 ;
  }

  p = kmalloc(sizeof(struct merchant_info_s),0L);

  if (!p)
    return -1 ;


  strncpy(p->id,merchant_id,sizeof(p->id));

  p->mch_info = mch_info ;

  p->pubkey  = alloc_default_dbuffer();
  p->privkey = alloc_default_dbuffer();

  // the public key
  pv = get_tree_map_value(p->mch_info,"pubkey");
  if (pv) {
    write_dbuf_str(p->pubkey,pv);
  }

  // the private key
  pv = get_tree_map_value(p->mch_info,"privkey");
  if (pv) {
    write_dbuf_str(p->privkey,pv);
  }


  p->verify_sign = false ;

  // is verify sign
  pv = get_tree_map_value(p->mch_info,"verify_sign");
  if (pv) {
    p->verify_sign = pv[0]=='1';
  }


  p->sign_type = MD_MD5;

  // sign type
  pv = get_tree_map_value(p->mch_info,"sign_type");
  if (pv) {
    if (!strcasecmp(pv,"md5")) p->sign_type = MD_MD5;
    else if (!strcasecmp(pv,"sha1")) p->sign_type = MD_SHA1;
    else if (!strcasecmp(pv,"sha224")) p->sign_type = MD_SHA224;
    else if (!strcasecmp(pv,"sha256")) p->sign_type = MD_SHA256;
    else if (!strcasecmp(pv,"sha384")) p->sign_type = MD_SHA384;
    else if (!strcasecmp(pv,"sha512")) p->sign_type = MD_SHA512;
  }

  INIT_LIST_HEAD(&p->alipay_pay_route);
  INIT_LIST_HEAD(&p->alipay_transfund_route);

  // merchant-based pay route
  pv = get_tree_map_value(p->mch_info,"pay_chan_ids");
  if (pv && strlen(pv)>0) {
    init_pay_route_references(pe,&p->alipay_pay_route,pv,false);
    log_info("adding pay channels '%s' for mch '%s'\n",pv,p->id);
  }
  else {
    log_error("no pay route is configure for merchant '%s'\n",p->id);
  }

  // merchant-based transfund route
  pv = get_tree_map_value(p->mch_info,"transfund_chan_ids");
  if (pv && strlen(pv)>0) {
    init_pay_route_references(pe,&p->alipay_transfund_route,pv,true);
    log_info("adding transfund channels '%s' for mch '%s'\n",pv,p->id);
  }
  else {
    log_error("no transfund route is configure for merchant '%s'\n",p->id);
  }


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

  drop_dbuffer(p->pubkey);
  drop_dbuffer(p->privkey);

  release_all_pay_route_references(&p->alipay_pay_route);
  release_all_pay_route_references(&p->alipay_transfund_route);

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
  tree_map_t pr  = get_running_configs()->mch_conf;
  tree_map_t mcfg= get_tree_map_nest(pr,"merchants");


  init_merchant_entry(pm,-1);

  rbtree_postorder_for_each_entry_safe(pos,n,&mcfg->u.root,node) {
    tree_map_t mch_map = pos->nest_map;
    char *mch_id = get_tree_map_value(mch_map,"name") ;

    save_merchant(pm,mch_id,mch_map);
  }

  return 0;
}

