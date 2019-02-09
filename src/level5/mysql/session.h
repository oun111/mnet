#ifndef __SESSION_H__
#define __SESSION_H__

#include "rbtree.h"
#include "list.h"
#include "mysqls.h"
#include "dbuffer.h"
#include "objpool.h"


struct session_entry_s {
  union {
    struct rb_root root;
  } u;
  size_t num_sessions ;

  unsigned long long increasable_id ;

#if 0
  struct kmem_cache *session_cache;
  struct list_head free_session_pool;
#else
  objPool_t pool ;
#endif
} ;

typedef struct session_entry_s* session_entry_t ;


struct client_session_s {
  int fd ;
  char scramble[AP_LENGTH];
  size_t sc_len ;
  struct rb_node node ;
  int status ; // 0 init, 1 login ok, 2 connect to server ok

#if 0
  dbuffer_t db;
  dbuffer_t usr;
  dbuffer_t pwd;
  dbuffer_t addr;
#else
  char db[32];
  char usr[32];
  char pwd[32];
  char addr[48];
#endif

  struct last_command_info_s {
    char id[16];
    char s_times[48];
#if 0
    dbuffer_t desc ;
    dbuffer_t stat ;
    dbuffer_t info ;
#else
    char desc[32] ;
    char stat[32] ;
    char info[48] ;
#endif
    long long times ;
  } cmd ;

  struct list_head pool_item ;
} ;

typedef struct client_session_s* session_t ;

enum cmdStat
{
  st_idle,
  st_qry,
  st_error,
} ;


extern int init_session_entry(session_entry_t, const ssize_t);

extern int release_all_sessions(session_entry_t entry);

extern void drop_session(session_entry_t, session_t);

extern session_t create_empty_session(session_entry_t, int);

extern session_t get_session(session_entry_t, int);

extern int reset_session(session_t);

extern int update_session(session_t, char*, char*, char*, int) ;

extern int set_session_last_command(session_entry_t entry, int fd, 
    int stat, char *stmt, size_t szStmt);

extern size_t get_num_sessions(session_entry_t entry);

#endif /* __SESSION_H__*/
