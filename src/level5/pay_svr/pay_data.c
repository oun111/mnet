#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "pay_data.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "config.h"


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


pay_channels_entry_t new_pay_channels_entry()
{
  pay_channels_entry_t entry = kmalloc(sizeof(struct pay_channels_entry_s),0L);


  entry->u.root = RB_ROOT ;

  return entry;
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

pay_data_t 
add_pay_data(pay_channels_entry_t entry, const char *chan, 
             const char *subname, tree_map_t params)
{
  char *pv = 0;
  pay_channel_t pc = get_pay_channel(entry,chan);
  pay_data_t p = NULL;


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

    p->subname = alloc_default_dbuffer(chan);
    p->pay_params = params;
    p->rc.max_amount = 0.0;
    p->rc.max_orders = 0;
    p->rc.time = 0L;

    INIT_LIST_HEAD(&p->upper); 
    list_add(&p->upper,&pc->pay_data_list);
  }

  pv = (char*)subname ;
  write_dbuf_str(p->subname,pv);

  p->is_online = true;

  return p;
}

static
int drop_pay_data_internal(pay_channel_t pc)
{
  pay_data_t pos,n ;


  list_for_each_entry_safe(pos,n,&pc->pay_data_list,upper) {
    drop_dbuffer(pos->subname);
    kfree(pos);
  }

  return 0;
}

int drop_pay_channel(pay_channels_entry_t entry, const char *chan)
{
  pay_channel_t pc = get_pay_channel(entry,chan);


  if (!pc) {
    return -1;
  }

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


  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_pay_channel(entry,pos->channel);
  }

  return 0;
}

void delete_pay_channels_entry(pay_channels_entry_t entry)
{
  release_all_pay_datas(entry);

  kfree(entry);
}

void update_paydata_rc_arguments(pay_data_t pd, double amount)
{
  struct timespec ts ;
  clock_gettime(CLOCK_REALTIME,&ts);


  if ((ts.tv_sec-pd->rc.time)<60) {
    pd->rc.max_amount += amount ;
    pd->rc.max_orders ++ ;
  }
}

static int get_rc_paras(tree_map_t rc_cfg, struct risk_control_s *rcp, dbuffer_t *reason)
{
  // max order count 
  char *rck = "max_orders" ;
  char *tmp = get_tree_map_value(rc_cfg,rck);
  char msg[256] = "";

  if (!tmp) {
    snprintf(msg,sizeof(msg),"risk control keyword '%s' not found",rck);
    log_error("%s\n",msg);
    write_dbuf_str(*reason,msg);
    return -1 ;
  }
  rcp->max_orders = atoi(tmp);

  // max amount 
  rck = "max_amount" ;
  tmp = get_tree_map_value(rc_cfg,rck);
  if (!tmp) {
    snprintf(msg,sizeof(msg),"risk control keyword '%s' not found",rck);
    log_error("%s\n",msg);
    write_dbuf_str(*reason,msg);
    return -1 ;
  }
  rcp->max_amount = atof(tmp);

  return 0;
}

pay_data_t get_pay_route(pay_channels_entry_t entry, const char *chan, dbuffer_t *reason)
{
  extern paySvr_config_t get_running_configs();
  paySvr_config_t conf = get_running_configs();
  pay_channel_t pc  = get_pay_channel(entry,chan);
  // get channel related risk control configs
  tree_map_t rc_cfg = get_rc_conf_by_channel(conf,chan);
  pay_data_t pos ;
  struct risk_control_s rc_cfg_paras ;
  char msg[256] = "";


  if (!pc) {
    snprintf(msg,sizeof(msg),"no pay channel '%s'",chan);
    log_error("%s\n",msg);
    write_dbuf_str(*reason,msg);
    return NULL ;
  }

  if (!rc_cfg) {
    snprintf(msg,sizeof(msg),"no risk control params for channel '%s'",chan);
    log_error("%s\n",msg);
    write_dbuf_str(*reason,msg);
    return NULL ;
  }

  // risk control timestamp
  struct timespec ts ;
  clock_gettime(CLOCK_REALTIME,&ts);

  // risk control arguments
  if (get_rc_paras(rc_cfg,&rc_cfg_paras,reason)) {
    return NULL;
  }

  //log_debug("max_orders: %d, max_amount: %f\n",max_orders,max_amount);


  // get best pay route
  list_for_each_entry(pos,&pc->pay_data_list,upper) {
    if (pos->is_online==false)
      continue ;

    if (pos->rc.time==0L || (ts.tv_sec-pos->rc.time)>60) {
      pos->rc.time = ts.tv_sec;
      pos->rc.max_orders = 0;
      pos->rc.max_amount = 0.0;
    }

    if ((ts.tv_sec-pos->rc.time)<60) {
      // 
      if (pos->rc.max_orders>=rc_cfg_paras.max_orders)
        continue ;

      if (pos->rc.max_amount>=rc_cfg_paras.max_amount)
        continue ;
    }

    // move pay data item to list tail
    {
      list_del(&pos->upper);
      list_add_tail(&pos->upper,&pc->pay_data_list);
    }

    return pos ;
  }

  snprintf(msg,sizeof(msg),"no suitable pay route for channel '%s'",chan);
  log_error("%s\n",msg);
  write_dbuf_str(*reason,msg);

  return NULL ;
}

int init_pay_data(pay_channels_entry_t *paych)
{
  tm_item_t pos,n;
  tm_item_t pos1,n1;
  extern paySvr_config_t get_running_configs();
  tree_map_t pr = get_running_configs()->chan_conf;
  tree_map_t entry = get_tree_map_nest(pr,"channels");
  
  
  *paych = new_pay_channels_entry();

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    tree_map_t chansub = pos->nest_map ;

    if (!chansub)
      continue ;

    rbtree_postorder_for_each_entry_safe(pos1,n1,&chansub->u.root,node) {
      add_pay_data(*paych,pos->key,pos1->key,pos1->nest_map);
      log_debug("adding channel '%s - %s'\n",pos->key,pos1->key);
    }
  }

  return 0;
}

