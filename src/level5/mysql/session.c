#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "session.h"
#include "log.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"


static int compare(int fd0, int fd1)
{
  return fd0>fd1?1:fd0<fd1?-1:0;
}

static
int __session_pool_init(session_entry_t entry, size_t pool_size)
{
  /* FIXME: */
  entry->pool = create_obj_pool("mp2-session-pool",pool_size,struct client_session_s);

#if 0
  session_t pos,n ;

  list_for_each_objPool_item(pos,n,entry->pool) {
    pos->db  = alloc_default_dbuffer();
    pos->usr = alloc_default_dbuffer();
    pos->pwd = alloc_default_dbuffer();
    pos->addr= alloc_default_dbuffer();

    pos->cmd.desc = alloc_default_dbuffer();
    pos->cmd.stat = alloc_default_dbuffer();
    pos->cmd.info = alloc_default_dbuffer();
  }
#endif

  log_info("session pool size: %zu\n",pool_size);

  return 0;
}

static
int __session_pool_destroy(session_entry_t entry)
{
#if 0
  session_t pos, n;

  list_for_each_objPool_item(pos,n,entry->pool) {
    drop_dbuffer(pos->db);
    drop_dbuffer(pos->usr);
    drop_dbuffer(pos->pwd);
    drop_dbuffer(pos->addr);

    drop_dbuffer(pos->cmd.desc);
    drop_dbuffer(pos->cmd.stat);
    drop_dbuffer(pos->cmd.info);
  }
#endif

  release_obj_pool(entry->pool,struct client_session_s);

  return 0;
}

int init_session_entry(session_entry_t entry, const size_t pool_size)
{
  entry->u.root = RB_ROOT ;
  entry->num_sessions = 0L;
  entry->increasable_id = 0L;

  __session_pool_init(entry,pool_size);
  return 0;
}

session_t create_empty_session(session_entry_t entry, int cid)
{
  session_t s = 0;

  /* already exists */
  if (!MY_RB_TREE_FIND(&entry->u.root,cid,s,fd,node,compare)) {
  //if (!MY_RB_TREE_FIND(&entry->u.root,cid,s,fd,node)) {
    log_info("client %d session already exists\n",cid);
    return s;
  }

  s = obj_pool_alloc(entry->pool,struct client_session_s);
  if (!s) {
    log_error("cant create more sessions!!\n");
    return NULL;
  }

  mysqls_gen_rand_string(s->scramble,AP_LENGTH-1);
  s->scramble[AP_LENGTH] = '\0';
  s->sc_len = AP_LENGTH-1;
  s->fd = cid;
  s->status = 0;
  s->cmd.times= 0L;

  snprintf(s->cmd.id,8,"%llu",__sync_fetch_and_add(&entry->increasable_id,1));

  if (MY_RB_TREE_INSERT(&entry->u.root,s,fd,node,compare)) {
  //if (MY_RB_TREE_INSERT(&entry->u.root,s,fd,node)) {
    log_error("insert client %d session fail\n",cid);
    obj_pool_free(entry->pool,s);
    return NULL;
  }

  __sync_fetch_and_add(&entry->num_sessions,1);

  return s;
}

int release_all_sessions(session_entry_t entry)
{
  session_t pos;
  session_t n ;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_session(entry,pos);
  }

  __session_pool_destroy(entry);

  return 0;
}

void drop_session(session_entry_t entry, session_t s)
{
  rb_erase(&s->node,&entry->u.root);

  obj_pool_free(entry->pool,s);

  __sync_fetch_and_sub(&entry->num_sessions,1);
}

session_t get_session(session_entry_t entry, int cid)
{
  session_t s = NULL ;
  
  MY_RB_TREE_FIND(&entry->u.root,cid,s,fd,node,compare) ;
  //MY_RB_TREE_FIND(&entry->u.root,cid,s,fd,node) ;
  return s ;
}

int reset_session(session_t s)
{
  s->status = 0;
  return 0;
}

int update_session(session_t s, char *db, char *usr, 
    char *pwd, int fd)
{
  struct sockaddr_in sa ;
  socklen_t ln = sizeof(sa) ;
  char chPort[48], *addr= 0;

  /* client address & port */
  getpeername(fd,(struct sockaddr*)&sa,&ln);
  addr = inet_ntoa(sa.sin_addr) ;
  sprintf(chPort,"%s:%d",addr,sa.sin_port);

#if 0
  s->db  = write_dbuffer(s->db,db,strlen(db));
  s->usr = write_dbuffer(s->usr,usr,strlen(usr));
  s->pwd = write_dbuffer(s->pwd,pwd,strlen(pwd));
  s->addr= write_dbuffer(s->addr,addr,strlen(addr));
  s->addr= append_dbuffer(s->addr,chPort,strlen(chPort));
#else
  strncpy(s->db,db,sizeof(s->db));
  strncpy(s->usr,usr,sizeof(s->usr));
  strncpy(s->pwd,pwd,sizeof(s->pwd));
  strncpy(s->addr,chPort,sizeof(s->addr));
#endif
  s->status = 1;

  return 0;
}

int set_session_last_command(session_entry_t entry, int fd, 
    int stat, char *stmt, size_t szStmt)
{
  session_t s = get_session(entry, fd);

  if (!s)
    return -1;

  /* idle */
  if (stat==st_idle) {
#if 0
    s->cmd.desc  = write_dbuffer(s->cmd.desc,"Sleep",6);
    s->cmd.stat = write_dbuffer(s->cmd.stat," ",1);
    s->cmd.info = write_dbuffer(s->cmd.info,"NULL",5);
#else
    strcpy(s->cmd.desc,"Sleep");
    strcpy(s->cmd.stat," ");
    strcpy(s->cmd.info,"NULL");
#endif
  }
  /* error */
  else if (stat==st_error) {
#if 0
    s->cmd.desc  = write_dbuffer(s->cmd.desc,"Init",5);
    s->cmd.stat = write_dbuffer(s->cmd.stat,"ERROR",6);
    s->cmd.info = write_dbuffer(s->cmd.info,stmt,strlen(stmt));
    s->cmd.info[strlen(stmt)] = '\0';
#else
    strcpy(s->cmd.desc,"Init");
    strcpy(s->cmd.stat,"ERROR");
    strncpy(s->cmd.info,stmt,sizeof(s->cmd.info));
#endif
  } 
  /* query */
  else {
#if 0
    s->cmd.desc  = write_dbuffer(s->cmd.desc,"Init",5);
    s->cmd.stat = write_dbuffer(s->cmd.stat,"Query",6);
    s->cmd.info = write_dbuffer(s->cmd.info,stmt,strlen(stmt));
    s->cmd.info[strlen(stmt)] = '\0';
#else
    strcpy(s->cmd.desc,"Init");
    strcpy(s->cmd.stat,"Query");
    strncpy(s->cmd.info,stmt,sizeof(s->cmd.info));
#endif
  }

  s->cmd.times = time(NULL);
  return 0;
}

size_t get_num_sessions(session_entry_t entry)
{
  return entry->num_sessions;
}

