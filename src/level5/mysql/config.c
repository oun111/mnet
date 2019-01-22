#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "file_cache.h"
#include "mm_porting.h"
#include "log.h"
#include "myrbtree.h"
#include "socket.h"


#define CHECK_DUPLICATE(list,item,member) ({ \
  int dup = 0;   \
  jsonKV_t *p = 0;  \
  list_for_each_entry(p,(list),member) \
    if (p!=item && !strcmp(p->key,item->key)) {\
      dup = 1; \
      break; \
    } \
  dup ;\
})

static const  
struct global_config_keywords {
  const char *dnSec;
  const char *dbAddr ;
  const char *dbSchema ;
  const char *dbAuth ;
  const char *rgnMap ;
  const char *modN ;
  const char *defDn ;
  const char *rgns ;
  const char *schemaSec ;
  const char *tableSec ;
  const char *mapSec ;
  const char *shdSec ;
  const char *globalIdSec ;
  const char *ruleSec ;
  const char *intvSec ;
  const char *ioTypeSec ;
  const char *gsSec ;
  const char *szCachePool ;
  const char *szDnGrp ;
  const char *idleSecs ;
  const char *bndAddrSec ;
  const char *lstnPortSec ;
  const char *maxBkTskSec ;
} 
g_confKW = {
  .dnSec       = "DataNodes",
  .tableSec    = "Tables",
  .mapSec      = "Mappings",
  .shdSec      = "shardingKey",
  .ioTypeSec   = "IoType",
  .ruleSec     = "rule",
  .schemaSec   = "Schemas",
  .globalIdSec = "globalIdColumn",
  .intvSec     = "interval",
  .gsSec       = "Globals",
  .rgnMap      = "rangeMap",
  .modN        = "modN",
  .defDn       = "defDn",
  .rgns        = "ranges",
  .dbAddr      = "Address",
  .dbSchema    = "Schema",
  .dbAuth      = "Auth",
  .szCachePool = "CachePoolSize",
  .szDnGrp     = "DatanodeGroupCount",
  .idleSecs    = "IdleSeconds",
  .bndAddrSec  = "BindAddr",
  .lstnPortSec = "ListenPort",
  .maxBkTskSec = "MaxBackendTaskCount",
} ;


static 
data_node_t new_data_node()
{
  data_node_t dn = kmalloc(sizeof(struct data_node_s),0L);

  dn->num = -1;
  dn->address[0] = '\0';
  dn->port = 0;
  dn->name = alloc_default_dbuffer();
  dn->schema = alloc_default_dbuffer();
  dn->auth.usr = alloc_default_dbuffer();
  dn->auth.pwd = alloc_default_dbuffer();
  INIT_LIST_HEAD(&dn->upper);
  INIT_LIST_HEAD(&dn->auth.upper);

  return dn ;
}

static
int free_data_node(data_node_t dn)
{
  drop_dbuffer(dn->name);
  drop_dbuffer(dn->schema);
  drop_dbuffer(dn->auth.usr);
  drop_dbuffer(dn->auth.pwd);
  list_del(&dn->upper);
  /* dn->auth.upper is not used */

  kfree(dn);

  return 0;
}

static 
auth_block_t new_auth_block()
{
  auth_block_t pa = kmalloc(sizeof(struct auth_block_s),0L);

  pa->usr = alloc_default_dbuffer();
  pa->pwd = alloc_default_dbuffer();
  INIT_LIST_HEAD(&pa->upper);

  return pa ;
}

static
int free_auth_block(auth_block_t pa)
{
  drop_dbuffer(pa->usr);
  drop_dbuffer(pa->pwd);
  list_del(&pa->upper);

  kfree(pa);

  return 0;
}

static 
mapping_info_t new_mapping()
{
  mapping_info_t pm = kmalloc(sizeof(struct mapping_info_s),0L);

  pm->datanode = alloc_default_dbuffer();
  pm->io_type  = it_both;
  INIT_LIST_HEAD(&pm->upper);

  return pm ;
}

static
int free_mapping(mapping_info_t pm)
{
  drop_dbuffer(pm->datanode);
  list_del(&pm->upper);

  kfree(pm);

  return 0;
}

static 
table_info_t new_table()
{
  table_info_t pt = kmalloc(sizeof(struct table_info_s),0L);

  pt->name = alloc_default_dbuffer();
  INIT_LIST_HEAD(&pt->upper);
  INIT_LIST_HEAD(&pt->map_list);

  return pt ;
}

static
int free_table(table_info_t pt)
{
  mapping_info_t pos=0, n=0;

  drop_dbuffer(pt->name);
  list_del(&pt->upper);

  list_for_each_entry_safe(pos,n,&pt->map_list,upper) 
    free_mapping(pos);

  kfree(pt);

  return 0;
}

static 
schema_t new_schema()
{
  schema_t sc = kmalloc(sizeof(struct schema_s),0L);

  sc->name = alloc_default_dbuffer();
  INIT_LIST_HEAD(&sc->upper);
  INIT_LIST_HEAD(&sc->auth_list);
  INIT_LIST_HEAD(&sc->table_list);
  sc->num_tables = 0L;

  return sc ;
}

static
int free_schema(schema_t sc)
{
  auth_block_t pos=0, n=0;
  table_info_t pos0=0, n0=0 ;

  drop_dbuffer(sc->name);
  list_del(&sc->upper);

  list_for_each_entry_safe(pos,n,&sc->auth_list,upper) 
    free_auth_block(pos);

  list_for_each_entry_safe(pos0,n0,&sc->table_list,upper) 
    free_table(pos0);

  kfree(sc);

  return 0;
}

static 
int parse_data_nodes(config_t conf)
{
  char *pv = 0;
  size_t vl = 0L;
  jsonKV_t *pd = jsons_find(conf->m_root,g_confKW.dnSec), 
      *item= 0;
  int dn_num = 0;

  if (!pd) {
    log_error("entry '%s' not found\n",g_confKW.dnSec);
    return -1;
  }

  list_for_each_entry(item,&pd->children,upper) {
    /* FIXME */
    if (CHECK_DUPLICATE(&pd->children,item,upper))
      continue ;

    data_node_t dn = new_data_node();

    dn->num  = dn_num++ ;
    pv = jsons_string(item->key,&vl);
    dn->name = write_dbuffer(dn->name,pv,vl);
    dn->name[vl] = '\0';

    /* data node address */
    jsonKV_t *tmp = jsons_find(item,g_confKW.dbAddr);
    char *pcol = NULL, ch = 0;
    const char *paddr = "127.0.0.1";

    if (tmp) {
      pv    = jsons_string(tmp->value,&vl);
      pcol  = strchr(pv,':');
      paddr = pv ;
      ch    = pv[vl] ;
      pv[vl]= '\0'; ;

      if (!pcol) {
        strncpy(dn->address, paddr, sizeof(dn->address));
        dn->port = 3306 ;
      } else {
        *pcol = '\0';
        strncpy(dn->address, paddr, sizeof(dn->address));
        dn->port = atoi(pcol+1);
        *pcol = ':';
      }

      pv[vl] = ch ;
    }

    /* related schema name */
    tmp = jsons_find(item,g_confKW.dbSchema);
    if (!tmp) {
      log_error("entry '%s' not found\n",g_confKW.dbSchema);
      free_data_node(dn);
      return -1;
    }
    pv = jsons_string(tmp->value,&vl);
    dn->schema = write_dbuffer(dn->schema,pv,vl);
    dn->schema[vl] = '\0';

    /* authentications */
    tmp = jsons_find(item,g_confKW.dbAuth);
    if (!tmp) {
      log_error("entry '%s' not found\n",g_confKW.dbAuth);
      free_data_node(dn);
      return -1;
    }

    jsonKV_t *p = 0;

    list_for_each_entry(p,&tmp->children,upper) {
      pv = jsons_string(p->key,&vl);
      dn->auth.usr = write_dbuffer(dn->auth.usr,pv,vl);
      dn->auth.usr[vl] = '\0';
      pv = jsons_string(p->value,&vl);
      dn->auth.pwd = write_dbuffer(dn->auth.pwd,pv,vl);
      dn->auth.pwd[vl] = '\0';
    }

    /* add to datanode list */
    list_add(&dn->upper,&conf->m_dataNodes);
  }

  return 0;
}

static
size_t str2id(const char *s)
{
  size_t ret=0;
  size_t i=0;
  const size_t sz = strlen(s);
  char *pEnd = (char*)(s+sz);

  for (char*p=(char*)s;p&&*p&&p<pEnd;p++,i++)
    ret += (*p^i) ;
  ret ^= (32&sz) ;

  return ret ;
}

static int compare(long long key0, long long key1)
{
  return key0>key1?1:key0<key1?-1:0;
}

static 
sharding_key_t new_sharding_key(config_t conf, char *sche, char *tbl, char *col)
{
  long long k = str2id(sche)<<16 | str2id(tbl) ;
  sharding_key_t pk = NULL ;
  
  if (MY_RB_TREE_FIND(&conf->m_shds,k,pk,key,node,compare)) 
    pk = kmalloc(sizeof(struct sharding_key_s),0L);

  pk->rule = t_dummy ;
  pk->sch  = alloc_default_dbuffer();
  pk->tbl  = alloc_default_dbuffer();
  pk->col  = alloc_default_dbuffer();

  pk->sch  = write_dbuffer(pk->sch,sche,strlen(sche));
  pk->tbl  = write_dbuffer(pk->tbl,tbl,strlen(tbl));
  pk->col  = write_dbuffer(pk->col,col,strlen(col));

  pk->key  = k;

  MY_RB_TREE_INSERT(&conf->m_shds,pk,key,node,compare);

  return pk ;
}

static
int free_all_sharding_keys(config_t conf)
{
  sharding_key_t pos, n ;

  rbtree_postorder_for_each_entry_safe(pos,n,&conf->m_shds,node) {
    drop_dbuffer(pos->sch);
    drop_dbuffer(pos->tbl);
    drop_dbuffer(pos->col);

    /* special for 'range map' method */
    if (pos->rule==t_rangeMap) {
      range_map_item_t pi=0, ni=0 ;

      list_for_each_entry_safe(pi,ni,&pos->u.rgn_map.rgn_list,upper) 
        kfree(pi);
    }

    rb_erase(&pos->node,&conf->m_shds);

    kfree(pos);
  }

  return 0;
}

static 
global_id_t new_global_id(config_t conf, char *sche, char *tbl, char *col)
{
  long long k = str2id(sche)<<16 | str2id(tbl) ;
  global_id_t pg = NULL ;
  
  if (MY_RB_TREE_FIND(&conf->m_gids,k,pg,key,node,compare)) 
    pg = kmalloc(sizeof(struct global_id_s),0L);

  pg->sch  = alloc_default_dbuffer();
  pg->tbl  = alloc_default_dbuffer();
  pg->col  = alloc_default_dbuffer();

  pg->sch  = write_dbuffer(pg->sch,sche,strlen(sche));
  pg->tbl  = write_dbuffer(pg->tbl,tbl,strlen(tbl));
  pg->col  = write_dbuffer(pg->col,col,strlen(col));

  pg->key  = k;

  MY_RB_TREE_INSERT(&conf->m_gids,pg,key,node,compare);

  return pg ;
}

static
int free_all_global_ids(config_t conf)
{
  global_id_t pos, n ;

  rbtree_postorder_for_each_entry_safe(pos,n,&conf->m_gids,node) {
    drop_dbuffer(pos->sch);
    drop_dbuffer(pos->tbl);
    drop_dbuffer(pos->col);

    rb_erase(&pos->node,&conf->m_gids);

    kfree(pos);
  }

  return 0;
}

static
int is_range_map_duplicated(struct list_head *rgn_list, long long begin, long long end)
{
  /* FIXME: use linux interval-tree here? */
  range_map_item_t item = 0 ;

  list_for_each_entry(item,rgn_list,upper) {
    /* range is duplicated */
    if (!(end<item->begin || begin>item->end))
      return 1;
  }

  return 0;
}

static
int save_range_maps(jsonKV_t *p, range_map_t *rm)
{
  jsonKV_t *item = 0;

  list_for_each_entry(item,&p->children,upper) {

    long long begin=0L, end=0L ;
    range_map_item_t it = 0;
    char *pv=0,ch=0;
    size_t vl = 0L;

    pv = jsons_string(item->value,&vl);
    ch = pv[vl] ;
    pv[vl] = '\0';
    sscanf(pv,"%lld,%lld",&begin,&end);
    pv[vl] = ch ;

    if (is_range_map_duplicated(&rm->rgn_list,begin,end)) 
      continue ;

    it = kmalloc(sizeof(struct range_map_item_s),0L);
    it->dn = atoi(item->key);
    it->begin = begin;
    it->end   = end;
    INIT_LIST_HEAD(&it->upper);

    list_add(&it->upper,&rm->rgn_list);
  }

  return 0;
}

static 
int parse_sharding_keys(config_t conf, jsonKV_t *p, schema_t sc, table_info_t pt)
{
  jsonKV_t *pd = jsons_find(p,g_confKW.shdSec), 
      *item= 0, *tmp = 0;
  char *pv = 0;
  size_t vl = 0L;

  if (!pd) {
    log_error("entry '%s' not found\n",g_confKW.shdSec);
    return -1;
  }

  list_for_each_entry(item,&pd->children,upper) {
    /* FIXME */
    if (CHECK_DUPLICATE(&pd->children,item,upper))
      continue ;

    sharding_key_t pk = new_sharding_key(conf,sc->name,pt->name,item->key);

    /* the rule */
    tmp = jsons_find(p,g_confKW.ruleSec); 
    if (tmp) {
      pv = jsons_string(tmp->value,&vl);
      pk->rule = !strncmp(pv,g_confKW.rgnMap,vl)?t_rangeMap:
        !strncmp(pv,g_confKW.modN,vl)?t_modN:t_dummy ;
    }

    /* for 'range map' method */
    if (pk->rule == t_rangeMap) {

      /* ranges */
      tmp = jsons_find(p,g_confKW.rgns); 
      if (!tmp) {
        pk->rule = t_modN;
        continue ;
      }

      save_range_maps(tmp,&pk->u.rgn_map);

      /* default datanode  */
      tmp = jsons_find(p,g_confKW.defDn); 
      pk->u.rgn_map.def_dn = tmp?jsons_integer(tmp->value):0;
    }

  }

  return 0;
}

static 
int parse_global_ids(config_t conf, jsonKV_t *p, schema_t sc, table_info_t pt)
{
  jsonKV_t *pd = jsons_find(p,g_confKW.globalIdSec), 
      *item= 0, *tmp = 0;

  if (!pd) {
    log_error("entry '%s' not found\n",g_confKW.shdSec);
    return -1;
  }

  list_for_each_entry(item,&pd->children,upper) {
    /* FIXME */
    if (CHECK_DUPLICATE(&pd->children,item,upper))
      continue ;

    global_id_t pg = new_global_id(conf,sc->name,pt->name,item->key);

    /* the rule */
    tmp = jsons_find(p,g_confKW.intvSec); 
    if (tmp) 
      pg->interval = jsons_integer(tmp->value) ;
  }

  return 0;
}

static 
int parse_mapping_info(jsonKV_t *p, table_info_t pt)
{
  jsonKV_t *pd = jsons_find(p,g_confKW.mapSec), 
      *item= 0, *tmp = 0;
  char *pv = 0;
  size_t vl= 0L;

  if (!pd) {
    log_error("entry '%s' not found\n",g_confKW.mapSec);
    return -1;
  }

  list_for_each_entry(item,&pd->children,upper) {
    /* FIXME */
    if (CHECK_DUPLICATE(&pd->children,item,upper))
      continue ;

    mapping_info_t pm = new_mapping();

    pv = jsons_string(item->key,&vl);
    pm->datanode = write_dbuffer(pm->datanode,pv,vl);
    pm->datanode[vl] = '\0';

    tmp = jsons_find(item,g_confKW.ioTypeSec);
    if (tmp) {
      pv = jsons_string(tmp->value,&vl);
      pm->io_type = !strncmp(pv,"read",vl)?it_read:
        !strncmp(pv,"write",vl)?it_write:it_both ;
    }

    list_add(&pm->upper,&pt->map_list);
  }

  return 0;
}

static 
int parse_schemas(config_t conf)
{
  char *pv = 0;
  size_t vl= 0L;
  jsonKV_t *pd = jsons_find(conf->m_root,g_confKW.schemaSec), 
      *item= 0, *tmp = 0;

  if (!pd) {
    log_error("entry '%s' not found\n",g_confKW.schemaSec);
    return -1;
  }

  list_for_each_entry(item,&pd->children,upper) {
    /* FIXME */
    if (CHECK_DUPLICATE(&pd->children,item,upper))
      continue ;

    schema_t sc = new_schema();

    pv = jsons_string(item->key,&vl);
    sc->name = write_dbuffer(sc->name,pv,vl);
    sc->name[vl] = '\0';

    /* auth list */
    tmp = jsons_find(item,g_confKW.dbAuth);
    if (!tmp) {
      log_error("entry '%s' not found\n",g_confKW.dbAuth);
      free_schema(sc);
      return -1;
    }

    jsonKV_t *p = 0;
    list_for_each_entry(p,&tmp->children,upper) {

      auth_block_t pa = new_auth_block();

      pv = jsons_string(p->key,&vl);
      pa->usr = write_dbuffer(pa->usr,pv,vl);
      pa->usr[vl] = '\0';
      pv = jsons_string(p->value,&vl);
      pa->pwd = write_dbuffer(pa->pwd,pv,vl);
      pa->pwd[vl] = '\0';
      list_add(&pa->upper,&sc->auth_list);
    }

    /* table list */
    tmp = jsons_find(item,g_confKW.tableSec);
    if (!tmp) {
      log_error("entry '%s' not found\n",g_confKW.tableSec);
      free_schema(sc);
      return -1;
    }

    list_for_each_entry(p,&tmp->children,upper) {
      /* FIXME */
      if (CHECK_DUPLICATE(&tmp->children,p,upper))
        continue ;

      table_info_t pt = new_table();

      pv = jsons_string(p->key,&vl);
      pt->name = write_dbuffer(pt->name,pv,vl);
      pt->name[vl] = '\0';

      /* mapping list */
      parse_mapping_info(p,pt);

      /* sharding key list */
      parse_sharding_keys(conf,p,sc,pt);

      /* global id list */
      parse_global_ids(conf,p,sc,pt);

      list_add(&pt->upper,&sc->table_list);
      sc->num_tables++ ;
    }

    /* add to schema list */
    list_add(&sc->upper,&conf->m_schemas);
    conf->num_schemas++ ;
  }

  return 0;
}

static 
int parse_global_settings(config_t conf)
{
  char *pv = 0;
  size_t vl = 0L;
  jsonKV_t *pg = jsons_find(conf->m_root,g_confKW.gsSec), 
      *p= 0;
  global_setting_t ps = &conf->m_globSettings ;

  if (!pg) {
    log_error("entry '%s' not found\n",g_confKW.gsSec);
    return -1;
  }

  list_for_each_entry(p,&pg->children,upper) {

    pv = jsons_string(p->key,&vl);
    if (!strncmp(pv,g_confKW.szDnGrp,vl)) {
      ps->szDnGrp = jsons_integer(p->value);
      if (ps->szDnGrp<=0 || ps->szDnGrp>50)
        ps->szDnGrp = 1;
    }
    else if (!strncmp(pv,g_confKW.idleSecs,vl)) {
      ps->idleSecs = jsons_integer(p->value);
      if (ps->idleSecs<=0 || ps->idleSecs>3600)
        ps->idleSecs = 30 ;
    }
    else if (!strncmp(pv,g_confKW.bndAddrSec,vl)) {
      pv = jsons_string(p->value,&vl);
      strncpy(ps->bindAddr,pv,vl);
    }
    else if (!strncmp(pv,g_confKW.lstnPortSec,vl)) {
      ps->listenPort = jsons_integer(p->value);
    }
    else if (!strncmp(pv,g_confKW.maxBkTskSec,vl)) {
      ps->maxBkTasks = jsons_integer(p->value);
    }
  }

  return 0;
}

int get_max_backend_tasks(config_t conf)
{
  return conf->m_globSettings.maxBkTasks ;
}

size_t get_dn_group_count(config_t conf) 
{
  return conf->m_globSettings.szDnGrp ;
}

int get_idle_seconds(config_t conf) 
{
  return conf->m_globSettings.idleSecs ;
}

const char* get_bind_address(config_t conf)
{
  return conf->m_globSettings.bindAddr ;
}

int get_listen_port(config_t conf)
{
  return conf->m_globSettings.listenPort ;
}

schema_t get_schema(config_t conf, const char *sch)
{
  schema_t sc = 0;

  list_for_each_entry(sc,&conf->m_schemas,upper) {
    if (!strcmp(sc->name,sch))
      return sc;
  }

  log_error("find no schema named %s\n",sch);

  return NULL;
}

size_t get_num_schemas(config_t conf) 
{
  return conf->num_schemas ;
}

size_t get_num_tables(schema_t sc)
{
  return sc->num_tables;
}

const char* get_auth(schema_t sc, const char *usr)
{
  auth_block_t ab = 0;

  list_for_each_entry(ab,&sc->auth_list,upper) {
    if (!strcmp(ab->usr,usr))
      return ab->pwd;
  }
  return NULL;
}

const char* get_pwd(config_t conf, const char *db, const char *usr)
{
  schema_t sc = get_schema(conf,db);

  if (sc)
    return get_auth(sc,usr) ;

  return NULL ;
}

bool is_db_exists(config_t conf, const char *db)
{
  return !!get_schema(conf,db) ;
}

static
int parse_content(config_t conf)
{
  /* parse data nodes */
  if (parse_data_nodes(conf)) {
    return -1;
  }

  /* parse schemas */
  if (parse_schemas(conf)) {
    return -1;
  }

  /* parse global settings */
  if (parse_global_settings(conf)) {
    return -1;
  }

  return 0;
}

int init_config(config_t conf, const char *infile)
{
  dbuffer_t content = NULL ;
  int err = 0;


  if (load_file(infile,&content) || !(conf->m_root=jsons_parse(content))) 
    err = -1;

  drop_dbuffer(content);
  
  if (err)
    return -1;

  INIT_LIST_HEAD(&conf->m_dataNodes); 

  INIT_LIST_HEAD(&conf->m_schemas); 
  conf->num_schemas = 0L;

  conf->m_shds = RB_ROOT;

  conf->m_gids = RB_ROOT;

  if (parse_content(conf)) {
    log_error("parse config content fail\n");
    return -1;
  }

  return 0;
}

int free_config(config_t conf)
{
  data_node_t pos=0, n=0 ;
  schema_t pos0=0, n0=0 ;

  jsons_release(conf->m_root);

  list_for_each_entry_safe(pos,n,&conf->m_dataNodes,upper)
    free_data_node(pos);

  list_for_each_entry_safe(pos0,n0,&conf->m_schemas,upper)
    free_schema(pos0);
  conf->num_schemas = 0L;

  free_all_sharding_keys(conf);

  free_all_global_ids(conf);

  return 0;
}

