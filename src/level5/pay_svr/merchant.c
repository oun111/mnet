#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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


#define DEFAULT_MAX_AMT  128.0


static int compare_double(double d0, double d1)
{
  return d0>d1?1:d0<d1?-1:0 ;
}

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

static 
int next_amount(char *raw, char **p, char **e)
{
  *p = raw ;
  while (isspace(*(*p))) 
    (*p)++ ;

  *e = strchr(*p,',');

  if (*e==NULL)
    return -1;

  **e = '\0';

  return 0;
}

static 
int parse_amount_range(const char *raw, double *min, double *max)
{
  char  *p = 0, *e = 0;

  // min part
  if (next_amount((char*)raw,&p,&e)) {
    return -1;
  }

  *min = atof(p);
  *e   = ',';

  // max part
  p = 0;
  next_amount(e+1,&p,&e);
  *max = atof(p);

  return 0;
}

static 
int parse_time_range(const char *raw, int *min, int *max)
{
  char  *p = 0, *e = 0;

  // min part
  if (next_amount((char*)raw,&p,&e)) {
    return -1;
  }

  *min = atoi(p);
  *e   = ',';

  // max part
  p = 0;
  next_amount(e+1,&p,&e);
  *max = atoi(p);

  return 0;
}

static
int parse_amount_fixed(const char *raw, struct rb_root *map)
{
  char  *p = 0, *e = 0;
  fixed_amt famt = NULL ;
  int rc = 0;

  for (p=(char*)raw; rc==0; p=e+1) {
    double d = 0.0 ;

    rc = next_amount(p,&p,&e);
    d  = atof(p);
    if (d<=0)
      continue ;

    famt = kmalloc(sizeof(struct fixed_amount_s),0L);
    famt->amount = d;

    MY_RB_TREE_INSERT(map,famt,amount,node,compare_double);

    if (e)
      *e = ',';

  }

  return 0;
}

static 
void print_fixed_amounts(struct rb_root *map)
{
  fixed_amt pos, n;


  if (map->rb_node==NULL)
    return ;

  log_debug("fixed amounts are: \n");
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,map,node) {
    log_debug("  %.2f\n",pos->amount);
  }
}

static 
int release_fixed_amounts(struct rb_root *map)
{
  fixed_amt pos, n;


  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,map,node) {
    rb_erase(&pos->node,map);
    kfree(pos);
  }

  return 0;
}

static
int import_amount_restrictions(merchant_info_t p)
{
  char *pv = 0;


  p->amounts.method = 0 ;
  pv = get_tree_map_value(p->mch_info,"amount_check_method");
  if (pv && strlen(pv)>0) {
    int m = atoi(pv);
    p->amounts.method = m>1||m<0?0:m;
  }

  log_debug("amount check method: %d\n",p->amounts.method);

  // 1#: amount range per request
  p->amounts.max  = 0.0;
  p->amounts.max  = 0.0;
  pv = get_tree_map_value(p->mch_info,"amount_range");
  if (pv && strlen(pv)>0 && parse_amount_range(pv,&p->amounts.min,&p->amounts.max)) {
    log_error("invalid amount range config: '%s'\n",pv);
  }

  if (!(p->amounts.max>p->amounts.min && p->amounts.min>0.0)) {
    p->amounts.min = 0.01;
    p->amounts.max = DEFAULT_MAX_AMT;
    log_error("invalid pay amount range of merchant '%s', default to [%.2f,%.2f]\n",
              p->id,p->amounts.min,p->amounts.max);
  }

  log_debug("amount range: %.2f ~ %.2f\n",p->amounts.min,p->amounts.max);

  // 2#: fixed amounts
  p->amounts.fixed= RB_ROOT ;
  pv = get_tree_map_value(p->mch_info,"amount_fixed");
  if (pv && strlen(pv)>0 && parse_amount_fixed(pv,&p->amounts.fixed)) {
    log_error("invalid fixed amount config: '%s'\n",pv);
  }

  print_fixed_amounts(&p->amounts.fixed);

  return 0;
}

static
int import_tradetime_restrictions(merchant_info_t p)
{
  char *pv = 0;


  p->tt.t0 = p->tt.t1 = -1;
  pv = get_tree_map_value(p->mch_info,"trade_time_range");
  if (pv && strlen(pv)>0) {
    parse_time_range(pv,&p->tt.t0,&p->tt.t1) ;
  }

  if (!((p->tt.t1-p->tt.t0)>0)) {
    log_error("invalid trade time range [%d,%d] of merchant '%s', "
        "default to WHOLE DAY long!\n",p->tt.t0,p->tt.t1,p->id);
    p->tt.t0 = p->tt.t1 = -1;
  }

  log_debug("time range: %d ~ %d\n",p->tt.t0,p->tt.t1);

  return 0;
}

static
int import_pay_routes(merchant_info_t p)
{
  char *pv = 0;
  extern pay_channels_entry_t get_pay_channels_entry() ;
  pay_channels_entry_t pe = get_pay_channels_entry() ;


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

  return 0;
}

static
int import_cryptos(merchant_info_t p)
{
  char *pv = 0;


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

  return 0;
}

int 
save_merchant(merchant_entry_t entry, char *merchant_id, tree_map_t mch_info)
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
  p->mch_info = mch_info ;

  import_cryptos(p);

  import_pay_routes(p);

  import_amount_restrictions(p);

  import_tradetime_restrictions(p);

  if (MY_RB_TREE_INSERT(&entry->u.root,p,id,node,compare)) {
    log_error("insert merchant id %s fail\n",merchant_id);
    kfree(p);
    return -1;
  }

  log_debug("adding merchant '%s'\n",p->id);

  entry->num_merchants ++;

  return 0;
}

bool is_merchant_amount_valid(merchant_info_t pm, double amt, dbuffer_t *errbuf)
{
  char tmp[64] = "";


  // fixed amounts
  if (pm->amounts.method==1) {
    fixed_amt pos = NULL, n = NULL ;

    MY_RB_TREE_FIND(&pm->amounts.fixed,amt,pos,amount,node,compare_double);

    if (!pos) {
      *errbuf = alloc_default_dbuffer();
      write_dbuf_str(*errbuf,"");

      MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&pm->amounts.fixed,node) {
        if (dbuffer_data_size(*errbuf)>0)
          append_dbuf_str(*errbuf,",");
        snprintf(tmp,sizeof(tmp),"%.2f",pos->amount);
        append_dbuf_str(*errbuf,tmp);
      }

      return false ;
    }
  }
  // otherwise check range
  else  if (!(amt>=pm->amounts.min && amt<=pm->amounts.max)) {

    *errbuf = alloc_default_dbuffer();
    snprintf(tmp,sizeof(tmp),"%.2f",pm->amounts.min);
    write_dbuf_str(*errbuf,tmp);
    append_dbuf_str(*errbuf," ~ ");
    snprintf(tmp,sizeof(tmp),"%.2f",pm->amounts.max);
    append_dbuf_str(*errbuf,tmp);
    return false;
  }

  return true;
}

static
int drop_merchant_internal(merchant_entry_t entry, merchant_info_t p)
{
  rb_erase(&p->node,&entry->u.root);

  drop_dbuffer(p->pubkey);
  drop_dbuffer(p->privkey);

  release_all_pay_route_references(&p->alipay_pay_route);
  release_all_pay_route_references(&p->alipay_transfund_route);

  release_fixed_amounts(&p->amounts.fixed);

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

