#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/time.h>
#include "pay_data.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "config.h"
#include "bitmap64.h"


static int compare(const char *s0, const char *s1)
{
  return strcmp(s0,s1);
}

pay_channel_t get_pay_channel(pay_channels_entry_t entry, const char *chan)
{
  pay_channel_t p = 0;


  if (MY_RB_TREE_FIND(&entry->u.root,chan,p,channel,node,compare)) 
    return NULL ;

  return p;
}

pay_data_t get_pay_data(pay_channel_t pc, const char *subname)
{
  pay_data_t pd = 0;


  list_for_each_entry(pd,&pc->pay_data_list,upper) {
    if (!strcmp(pd->subname,subname))
      return pd;
  }

  return NULL ;
}

pay_data_t 
get_paydata_by_ali_appid(pay_channels_entry_t entry, const char *chan, 
                         const char *appid)
{
  pay_data_t pd = 0;
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return NULL;
  }

  list_for_each_entry(pd,&pc->pay_data_list,upper) {
    const char *ch_appid = get_tree_map_value(pd->pay_params,"app_id");

    if (appid && !strcmp(ch_appid,appid))
      return pd;
  }

  return NULL ;
}

pay_data_t 
get_paydata_by_id(pay_channels_entry_t entry, const char *chan, int id)
{
  pay_data_t pd = 0;
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return NULL;
  }

  list_for_each_entry(pd,&pc->pay_data_list,upper) {
    const char *p_id = get_tree_map_value(pd->pay_params,"id");

    if (p_id && atoi(p_id)==id)
      return pd;
  }

  return NULL ;
}

pay_channel_t new_pay_channel(const char *chan)
{
  char *pv  = NULL;
  pay_channel_t pc = kmalloc(sizeof(struct pay_channel_item_s),0L);


  pv = (char*)chan ;
  pc->channel = alloc_default_dbuffer();
  write_dbuf_str(pc->channel,pv);
  INIT_LIST_HEAD(&pc->pay_data_list);

  return pc ;
}

static int get_rc_paras(tree_map_t rc_cfg, struct risk_control_s *rcp, dbuffer_t *reason)
{
  char *tmp = 0, *rcid = 0;
#define FETCH_RC_KEY(rck,__rc_cfg,tmp,r) do{ \
  char msg[256] = ""; \
  tmp = get_tree_map_value(__rc_cfg,rck) ;\
  if (!tmp) { \
    snprintf(msg,sizeof(msg),"risk control keyword '%s' not found",rck); \
    log_error("%s\n",msg);  \
    if (r) write_dbuf_str(r,msg); \
    return -1 ; \
  } \
}while(0)

  FETCH_RC_KEY("rcid",rc_cfg,rcid,*reason);

  FETCH_RC_KEY("max_orders",rc_cfg,tmp,*reason);
  rcp->max_orders = atoi(tmp);

  // max amount 
  FETCH_RC_KEY("max_amount",rc_cfg,tmp,*reason) ;
  rcp->max_amount = atof(tmp);

  // period
  FETCH_RC_KEY("period",rc_cfg,tmp,*reason) ;
  rcp->period = atol(tmp);

  log_info("rcid: %s, max_orders: %d, max_amount: %f, period: %lld\n",
      rcid, rcp->max_orders, rcp->max_amount, rcp->period);

  return 0;
}

pay_data_t 
add_pay_data(pay_channels_entry_t entry, const char *chan, 
             const char *subname, tree_map_t params)
{
  char *pv = 0;
  pay_channel_t pc = get_pay_channel(entry,chan);
  pay_data_t p = NULL;
  char *rcid = 0;
  char *ppriv = 0, *ppub = 0;


  if (!pc) {
    pc = new_pay_channel(chan);
    if (MY_RB_TREE_INSERT(&entry->u.root,pc,channel,node,compare)) {
      log_error("insert pay channel item by name '%s' fail\n",chan);
      kfree(pc);
      return NULL;
    }
  }

  p = get_pay_data(pc,subname);

  if (!p) {
    p = kmalloc(sizeof(struct pay_data_item_s),0L);
    if (!p) {
      log_error("allocate new tree map item fail\n");
      return NULL ;
    }

    p->subname = alloc_default_dbuffer();
    //p->pay_params = params;
    p->rc.max_amount = 0.0;
    p->rc.max_orders = 0;
    p->rc.time = 0L;
    p->cfg_rc.max_amount = 0.0;
    p->cfg_rc.max_orders = 0;
    p->cfg_rc.time = 0L;
    p->rsa_cache.sign_item   = NULL;
    p->rsa_cache.versign_item= NULL;

    INIT_LIST_HEAD(&p->upper); 
    list_add(&p->upper,&pc->pay_data_list);
  }

  // update pay params
  p->pay_params = params;

  // update risk control configs
  rcid = get_tree_map_value(p->pay_params,"rcid");
  if (rcid) {
    extern paySvr_config_t get_running_configs();
    paySvr_config_t conf = get_running_configs();
    tree_map_t rc_cfg = get_rc_conf_by_rcid(conf,rcid);

    if (rc_cfg) {
      get_rc_paras(rc_cfg,&p->cfg_rc,NULL);
    }
    else {
      log_error("found no risk control configs for rcid: %s\n", rcid);
    }
  }

  // pre init the rsa entry
  ppub  = get_tree_map_value(p->pay_params,"public_key_path");
  ppriv = get_tree_map_value(p->pay_params,"private_key_path");
  if (ppub && ppriv) {
    release_rsa_entry(&p->rsa_cache);
    init_rsa_entry(&p->rsa_cache,ppub,ppriv);
  }

  pv = (char*)subname ;
  write_dbuf_str(p->subname,pv);

  return p;
}

static
int drop_pay_data_internal(pay_channel_t pc)
{
  pay_data_t pos,n ;


  list_for_each_entry_safe(pos,n,&pc->pay_data_list,upper) {
    drop_dbuffer(pos->subname);
    release_rsa_entry(&pos->rsa_cache);
    kfree(pos);
  }

  return 0;
}

static
int drop_pay_channel(pay_channels_entry_t entry, pay_channel_t pc)
{
  drop_pay_data_internal(pc);

  rb_erase(&pc->node,&entry->u.root);

  drop_dbuffer(pc->channel);
  kfree(pc);

  return 0;
}

static
int release_all_pay_datas(pay_channels_entry_t entry)
{
  pay_channel_t pos,n;


  //rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&entry->u.root,node) {
    drop_pay_channel(entry,pos);
  }

  return 0;
}

void delete_pay_channels_entry(pay_channels_entry_t entry)
{
  release_all_pay_datas(entry);
}

void update_paydata_rc_arguments(pay_data_t pd, double amount)
{
  pd->rc.max_amount += amount ;
  pd->rc.max_orders ++ ;
}

int reset_paydata_rc_arguments(pay_channels_entry_t entry, const char *chan)
{
  pay_data_t pd = 0;
  struct timeval ts ;
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return -1;
  }

  gettimeofday(&ts,NULL);

  list_for_each_entry(pd,&pc->pay_data_list,upper) {
    pd->rc.time = ts.tv_sec;
    pd->rc.max_orders = 0;
    pd->rc.max_amount = 0.0;
  }

  log_debug("reset rc arguments done!!\n");

  return 0 ;
}

pay_data_t get_pay_route2(struct list_head *pr_list, dbuffer_t *reason) 
{
  pay_route_item_t pr ;
  char msg[256] = "";


  // risk control timestamp
#if 0
  struct timespec ts ;
  clock_gettime(CLOCK_REALTIME,&ts);
#else
  struct timeval ts ;
  gettimeofday(&ts,NULL);
#endif

  // get best pay route
  list_for_each_entry(pr,pr_list,upper) {

    pay_data_t pos = pr->pdr ;

    if (pos->rc.time==0L || (ts.tv_sec-pos->rc.time)>pos->cfg_rc.period) {
      pos->rc.time = ts.tv_sec;
      pos->rc.max_orders = 0;
      pos->rc.max_amount = 0.0;
    }
    log_debug("config rc: %lld, %d, %f\n",pos->cfg_rc.period,pos->cfg_rc.max_orders,pos->cfg_rc.max_amount);
    log_debug("current rc: %ld, %d, %f\n",pos->rc.time,pos->rc.max_orders,pos->rc.max_amount);

    if ((ts.tv_sec-pos->rc.time)<pos->cfg_rc.period) {
      // 
      if (pos->rc.max_orders>=pos->cfg_rc.max_orders)
        continue ;

      if (pos->rc.max_amount>=pos->cfg_rc.max_amount)
        continue ;
    }

    // move pay data item to list tail
    {
      list_del(&pr->upper);
      list_add_tail(&pr->upper,pr_list);
    }

    return pos ;
  }

  snprintf(msg,sizeof(msg),"no suitable pay route");
  log_error("%s\n",msg);
  write_dbuf_str(*reason,msg);

  return NULL ;
}

int init_pay_data(pay_channels_entry_t paych)
{
  tm_item_t pos,n;
  tm_item_t pos1,n1;
  extern paySvr_config_t get_running_configs();
  tree_map_t pr = get_running_configs()->chan_conf;
  tree_map_t entry = get_tree_map_nest(pr,"channels");
  
  
  //paych->u.root = RB_ROOT ;

  //rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&entry->u.root,node) {
    tree_map_t chansub = pos->nest_map ;

    if (!chansub)
      continue ;

    //rbtree_postorder_for_each_entry_safe(pos1,n1,&chansub->u.root,node) {
    MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos1,n1,&chansub->u.root,node) {
      add_pay_data(paych,pos->key,pos1->key,pos1->nest_map);
      log_info("adding channel '%s - %s'\n",pos->key,pos1->key);
    }
  }

  return 0;
}

int drop_outdated_pay_data(pay_channels_entry_t entry)
{
  pay_channel_t pos,n;
  pay_data_t posd,nd ;
  extern paySvr_config_t get_running_configs();
  tree_map_t pr = get_running_configs()->chan_conf;
  tree_map_t cfg_entry = get_tree_map_nest(pr,"channels");


  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&entry->u.root,node) {

    //log_debug("scanning channel '%s'\n",pos->channel);

    // find existing chan latest configs
    tm_item_t ti = 0;
    
    // not found means the channel is outdated!
    MY_RB_TREE_FIND(&cfg_entry->u.root,pos->channel,ti,key,node,compare);
    if (!ti) {
      log_debug("droping outdated pay channel '%s'\n",pos->channel);
      drop_pay_channel(entry,pos);
      continue ;
    }

    tree_map_t chansub = ti->nest_map ;

    list_for_each_entry_safe(posd,nd,&pos->pay_data_list,upper) {

      //log_debug("scanning paydata '%s'\n",posd->subname);

      // find pay data item from latest configs
      tm_item_t pi = 0;

      // not found means this paydata is outdated!!
      MY_RB_TREE_FIND(&chansub->u.root,posd->subname,pi,key,node,compare);
      if (!pi) {
        log_debug("droping outdated pay data '%s'\n",posd->subname);

        list_del(&posd->upper);
        drop_dbuffer(posd->subname);
        release_rsa_entry(&posd->rsa_cache);
        kfree(posd);
      }
      // suppose the latest pay datas are added before
#if 0
      // if exists, refresh it
      else {
        add_pay_data(entry,pos->channel,posd->subname,pi->nest_map);
      }
#endif

    }
  }

  return 0;
}

static
int idStr_to_bits(const char *s, bitmap64_t bm)
{
  for (char *p=(char*)s,*pch = NULL;p && *p!='\0';) {
    int v0 = 0;

    pch = strchr(p,',');
    if (pch) {
      *pch= '\0';
      v0  = atoi(p);
      *pch= ',';
      p   = pch + 1;
    }
    else {
      v0 = atoi(p);
      p++;
    }

    bm64_set_bit(bm,v0);

    if (!pch) break ;
  }

  return 0;
}

int init_pay_route_references(pay_channels_entry_t pe, struct list_head *pr_list,
                              const char *ch_ids, bool istransfund)
{
  pay_route_item_t pr = NULL;
  pay_data_t pd = NULL;
  const char *chan = "alipay";
  struct bitmap64_s bm ;
  int id = 0;


  // bitmap with 5000+ bits available
  BITMAP64_INIT(&bm,5000,true);

  // set bitmap by channel id numbers
  idStr_to_bits(ch_ids,&bm);

  // test bits in bitmap 
  BITMAP64_FOR_EACH_BITS(id,&bm) {

    if (!bm64_test_block(&bm,id)) {
      id += 63 ;
      continue ;
    }

    if (bm64_test_bit(&bm,id) && (pd=get_paydata_by_id(pe,chan,id))) {

      char *tf = get_tree_map_value(pd->pay_params,"istransfund");

      // test for alipay transfund channel
      if ((istransfund && (!tf || *tf!='1')) || (!istransfund && (tf && *tf=='1')))
        continue;

      pr = kmalloc(sizeof(struct pay_route_item_s),0L);
      pr->pdr = pd ;
      list_add(&pr->upper,pr_list);
    }
  }

  return 0;
}

int release_all_pay_route_references(struct list_head *pr_list)
{
  pay_route_item_t pos, n;


  list_for_each_entry_safe(pos,n,pr_list,upper) {
    kfree(pos);
  }

  return 0;
}

