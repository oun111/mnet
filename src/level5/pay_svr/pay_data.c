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
#include "order.h"


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

pay_data_t get_pay_data(pay_channel_t pc, const char *appid)
{
  pay_data_t pd = 0;


  if (!appid) {
    log_error("invalid appid!\n");
    return NULL;
  }

  list_for_each_entry(pd,&pc->pay_data_list,upper) {
    if (!strcmp(pd->appid,appid))
      return pd;
  }

  return NULL ;
}

pay_data_t 
get_paydata_by_ali_appid(pay_channels_entry_t entry, const char *chan, 
                         const char *appid)
{
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return NULL;
  }

  return get_pay_data(pc,appid);
}

pay_data_t 
get_paydata_by_id(pay_channels_entry_t entry, const char *chan, 
                  int id, int *paytype)
{
  pay_data_t pd = 0;
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return NULL;
  }

  list_for_each_entry(pd,&pc->pay_data_list,upper) {

    // search id from external id list
    for (int i=0;i<t_max;i++) 
      if (pd->pt_desc.id[i]==id) {
        *paytype = i ;
        return pd ;
      }
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
  rcp->order.max = atoi(tmp);

  // max amount 
  FETCH_RC_KEY("max_amount",rc_cfg,tmp,*reason) ;
  rcp->amount.max = atof(tmp);

  // periods
  FETCH_RC_KEY("odr_period",rc_cfg,tmp,*reason) ;
  rcp->order.period = atol(tmp);

  FETCH_RC_KEY("amt_period",rc_cfg,tmp,*reason) ;
  rcp->amount.period = atol(tmp);

  log_info("rcid: %s, max_orders: %d(period: %lld), max_amount: %f(period: %lld)\n",
      rcid, rcp->order.max, rcp->order.period, rcp->amount.max, rcp->amount.period);

  return 0;
}

pay_data_t 
add_pay_data(pay_channels_entry_t entry, const char *chan, 
             const char *appid, tree_map_t params)
{
  int nv = 0;
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

  p = get_pay_data(pc,appid);

  if (!p) {
    p = kmalloc(sizeof(struct pay_data_item_s),0L);
    if (!p) {
      log_error("allocate new tree map item fail\n");
      return NULL ;
    }

    p->appid = alloc_default_dbuffer();
    //p->pay_params = params;
#if 0
    p->rc.max_amount = 0.0;
    p->rc.max_orders = 0;
    p->rc.time = 0L;
    p->cfg_rc.max_amount = 0.0;
    p->cfg_rc.max_orders = 0;
    p->cfg_rc.time = 0L;
    p->rsa_cache.sign_item   = NULL;
    p->rsa_cache.versign_item= NULL;
#else
    bzero(&p->rc,sizeof(p->rc));
    bzero(&p->cfg_rc,sizeof(p->cfg_rc));
    bzero(&p->rsa_cache,sizeof(p->rsa_cache));
#endif

    memset(p->pt_desc.id,-1,sizeof(p->pt_desc.id));

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

  // pay type
  pv = get_tree_map_value(p->pay_params,"pay_type");
  nv = pv?atoi(pv):-1;
  if (unlikely(!IS_ORDERTYPE_VALID(nv))) 
    log_error("invalid pay type %d\n", nv);
  else {
    // related record id
    pv = get_tree_map_value(p->pay_params,"id");
    p->pt_desc.id[nv] = atoi(pv) ;
  }

  pv = (char*)appid ;
  write_dbuf_str(p->appid,pv);

  return p;
}

static
int drop_pay_data_internal(pay_channel_t pc)
{
  pay_data_t pos,n ;


  list_for_each_entry_safe(pos,n,&pc->pay_data_list,upper) {
    drop_dbuffer(pos->appid);
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
#if 0
  pd->rc.max_amount += amount ;
  pd->rc.max_orders ++ ;
#else
  pd->rc.amount.max += amount ;
  pd->rc.order.max ++ ;
#endif
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
#if 0
    pd->rc.time = ts.tv_sec;
    pd->rc.max_orders = 0;
    pd->rc.max_amount = 0.0;
#else
    pd->rc.order.time = ts.tv_sec;
    pd->rc.order.max  = 0;
    pd->rc.amount.time= ts.tv_sec;
    pd->rc.amount.max = 0.0;
#endif
  }

  log_debug("reset rc arguments done!!\n");

  return 0 ;
}

int 
fetch_runtime_rc(pay_channels_entry_t entry, runtime_rc_t re, const char *chan)
{
  pay_data_t pd = 0;
  struct timeval tv;
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return -1;
  }

  gettimeofday(&tv,NULL);

  list_for_each_entry(pd,&pc->pay_data_list,upper) {
    // get latest local time, NOT from redis storage
    pd->rc.order.time = tv.tv_sec;
    pd->rc.amount.time = tv.tv_sec;
    re->fetch(re,rt_amount,pd->appid,&pd->rc.amount.max);
    re->fetch(re,rt_orders,pd->appid,&pd->rc.order.max);

    log_debug("%s: amount(%lu,%f), order(%lu,%d)\n",pd->appid,
        pd->rc.amount.time,pd->rc.amount.max,
        pd->rc.order.time,pd->rc.order.max);
  }

  log_debug("fetch runtime rc data done!!\n");

  return 0 ;
}

int 
save_runtime_rc(pay_channels_entry_t entry, runtime_rc_t re, const char *chan)
{
  pay_data_t pd = 0;
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    log_error("found no pay channel '%s'\n",chan);
    return -1;
  }

  if (!re->save) {
    log_error("no save_runtime_rc() handler!\n");
    return -1;
  }

  list_for_each_entry(pd,&pc->pay_data_list,upper) {

    // don't save time onto redis storage, it's useless

    re->save(re,rt_amount,pd->appid,&pd->rc.amount.max);

    re->save(re,rt_orders,pd->appid,&pd->rc.order.max);
  }

  //log_debug("save runtime rc data done!!\n");

  return 0 ;
}

pay_data_t get_pay_route2(struct list_head *pr_list, dbuffer_t *reason, unsigned int *pt) 
{
  pay_route_item_t pr ;
  char msg[256] = "";


  // risk control timestamp
  struct timeval ts ;
  gettimeofday(&ts,NULL);

  // get best pay route
  list_for_each_entry(pr,pr_list,upper) {

    pay_data_t pos = pr->pdr ;

    // order timer times out!
    if (pos->rc.order.time==0L || (ts.tv_sec-pos->rc.order.time)>pos->cfg_rc.order.period) {
      pos->rc.order.time = ts.tv_sec;
      pos->rc.order.max = 0;
      //log_error("reseting appid %s order rc\n",pos->appid);
    }
    // amount timer times out!
    if (pos->rc.amount.time==0L || (ts.tv_sec-pos->rc.amount.time)>pos->cfg_rc.amount.period) {
      pos->rc.amount.time = ts.tv_sec;
      pos->rc.amount.max = 0.0;
      //log_error("reseting appid %s amount rc\n",pos->appid);
    }
    log_debug("appid: %s\n",pos->appid);
    log_debug("  config rc: amount(%fy / %llds),order(%d / %llds)\n",
        pos->cfg_rc.amount.max,pos->cfg_rc.amount.period,
        pos->cfg_rc.order.max,pos->cfg_rc.order.period);
    log_debug("  current rc: amount(%fy, %lus),order(%d / %lus)\n",
        pos->rc.amount.max,ts.tv_sec-pos->rc.amount.time,
        pos->rc.order.max,ts.tv_sec-pos->rc.order.time);

    if ((ts.tv_sec-pos->rc.order.time)<pos->cfg_rc.order.period) {

      if (pos->rc.order.max>=pos->cfg_rc.order.max)
        continue ;
    }

    if ((ts.tv_sec-pos->rc.amount.time)<pos->cfg_rc.amount.period) {

      if (pos->rc.amount.max>=pos->cfg_rc.amount.max)
        continue ;
    }

    // get pay type
    *pt = pr->pt ;

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

  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&entry->u.root,node) {
    tree_map_t chansub = pos->nest_map ;

    if (!chansub)
      continue ;

    MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos1,n1,&chansub->u.root,node) {

      char *appid = get_tree_map_value(pos1->nest_map,"app_id");

      add_pay_data(paych,pos->key,/*pos1->key*/appid,pos1->nest_map);
      log_info("adding channel by appid '%s - %s'\n",pos->key,/*pos1->key*/appid);
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

      log_debug("scanning paydata by appid '%s'\n",posd->appid);

      // find pay data item from latest configs
      tm_item_t pi, ni;
      int pts = 0;
      unsigned short ids[t_max];

      memset(ids,-1,sizeof(ids));
      // not found means this paydata is outdated!!
      MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pi,ni,&chansub->u.root,node) {
        char *appid = get_tree_map_value(pi->nest_map,"app_id");
        char *pv = get_tree_map_value(pi->nest_map,"pay_type");
        int nv   = !pv?-1:atoi(pv);


        if (unlikely(!IS_ORDERTYPE_VALID(nv))) {
          log_error("pay type %d invalid!!\n",nv);
          break ;
        }

        if (!strcmp(appid,posd->appid)) {
          char *pid= get_tree_map_value(pi->nest_map,"id");
          ids[nv] = atoi(pid);
          pts ++ ;
        }
      }

      if (!pts) {
        log_debug("droping outdated pay data by appid '%s'\n",posd->appid);

        list_del(&posd->upper);
        drop_dbuffer(posd->appid);
        release_rsa_entry(&posd->rsa_cache);
        kfree(posd);
      }
      else 
        memcpy(posd->pt_desc.id,ids,sizeof(ids));

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
  int id = 0, pt = 0;


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

    if (bm64_test_bit(&bm,id) && (pd=get_paydata_by_id(pe,chan,id,&pt))) {

      char *tf = get_tree_map_value(pd->pay_params,"istransfund");

      // test for alipay transfund channel
      if ((istransfund && (!tf || *tf!='1')) || (!istransfund && (tf && *tf=='1')))
        continue;

      pr = kmalloc(sizeof(struct pay_route_item_s),0L);
      pr->pdr = pd ;
      pr->pt  = pt ; // id --> pay type
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

