#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "dbuffer.h"
#include "list.h"
#include "jsons.h"
#include "rbtree.h"


struct range_map_item_s {
  int dn;
  long long begin ;
  long long end ;
  struct list_head upper ;
} ;
typedef struct range_map_item_s* range_map_item_t ;

typedef struct shd_method_range_map_s {
  struct list_head rgn_list ;
  int def_dn ;
} range_map_t;

/* distribution rules */
enum tRules {
  t_dummy, /* dummy rule */
  t_modN, /* get data node by MOD(data-node-count) */
  t_rangeMap, /* column value range and data node mapping */
  t_maxRules,
} ;

struct sharding_key_s {
  long long key ;
  dbuffer_t sch ; //schema name
  dbuffer_t tbl ; //table name
  dbuffer_t col ; //table name

  unsigned char rule ;

  union {
    range_map_t rgn_map ;
  } u ;

  struct rb_node node ;
} ;
typedef struct sharding_key_s* sharding_key_t ;

struct global_id_s {
  long long key ;
  dbuffer_t sch ; //schema name
  dbuffer_t tbl ; //table name
  dbuffer_t col ; //table name

  int interval ;

  struct rb_node node ;
} ;
typedef struct global_id_s* global_id_t ;

struct mapping_info_s {
  dbuffer_t datanode ; //datanode name
  unsigned char io_type ;
  struct list_head upper ;
} ;
typedef struct mapping_info_s* mapping_info_t;

enum ioTypes {
  it_read,
  it_write,
  it_both
} ;

struct table_info_s {
  dbuffer_t name ; //table name
  struct list_head map_list ;
  struct list_head upper;
} ;
typedef struct table_info_s* table_info_t ;

struct auth_block_s {
  dbuffer_t usr ;
  dbuffer_t pwd ;
  struct list_head upper;
} ;
typedef struct auth_block_s* auth_block_t ;

struct schema_s {
  dbuffer_t name ; //schema name
  struct list_head auth_list ;
  struct list_head table_list ;
  size_t num_tables ;
  struct list_head upper ;
} ;
typedef struct schema_s* schema_t ;

struct data_node_s {
  int num ;
  dbuffer_t name ;
  char address[32] ;
  unsigned int port ;
  dbuffer_t schema ;
  struct auth_block_s auth ;
  struct list_head upper ;
};
typedef struct data_node_s* data_node_t;

struct global_setting_s {
  size_t szCachePool ;
  size_t szDnGrp ;
  int idleSecs ;
  char bindAddr[32];
  int listenPort ;
  size_t maxBkTasks;
} ;
typedef struct global_setting_s* global_setting_t ;

struct config_s {

  jsonKV_t *m_root ;

  struct list_head m_dataNodes;

  struct list_head m_schemas;
  size_t num_schemas ;

  struct global_setting_s m_globSettings;
  
  struct rb_root m_shds;
  
  struct rb_root m_gids;

} __attribute__((__aligned__(64))) ;

typedef struct config_s* config_t ;


extern int get_max_backend_tasks(config_t conf);

extern size_t get_dn_group_count(config_t conf) ;

extern int get_idle_seconds(config_t conf) ;

extern schema_t get_schema(config_t conf, const char *sch);

extern size_t get_num_schemas(config_t conf) ;

extern size_t get_num_tables(schema_t sc);

extern bool is_db_exists(config_t conf, const char *db);

extern const char* get_pwd(config_t conf, const char *db, const char *usr);

extern const char* get_bind_address(config_t conf);

extern int get_listen_port(config_t conf);

extern int init_config(config_t conf, const char *infile);

extern int free_config(config_t conf);

#endif /* __CONFIG_H__*/

