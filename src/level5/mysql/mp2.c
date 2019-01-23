#define _GNU_SOURCE
#include <stdlib.h>
#include <linux/limits.h>
//#include <getopt.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "dbuffer.h"
#include "session.h"
#include "log.h"
#include "mp2.h"
#include "mysql_porting.h"
#include "config.h"
#include "instance.h"
#include "socket.h"
#include "backend.h"
#include "task.h"
#include "module.h"

#define MAX_PAYLOAD   1600


/* statement types */
enum stmt_type {
  s_na,

  /* statement should be executed at backends */
  s_sharding,

  /* the 'show databases' statements */
  s_show_dbs,

  /* the 'show tables' statements */
  s_show_tbls,

  /* the 'show processlist' statements */
  s_show_proclst,

  /* 'select version-comment' statement */
  s_sel_ver_comment,

  /* 'select DATABASE()' statement */
  s_sel_cur_db,

  s_max,
} ;

enum opt_bits {
  /* be executed on single backend, the route is well-calculated */
  s_single = 0x1,
};

#define STMT_TYPE(t,o)     (((o)<<16) | t)
#define GET_STMT_TYPE(v)   ((v)&0xffff)
#define GET_OPT_BITS(v)    (((v)>>16)&0xffff)


struct __attribute__((__aligned__(64))) level5_instance_s {

  int listenfd ;

  struct session_entry_s __sessions ;

  size_t max_sessions;

  size_t max_backend_tasks ;

  size_t max_dn_groups ;

  struct tCommandNode {
    int(*ha)(Network_t,connection_t,char*,size_t);
  }  m_handlers[com_end] ;

  int status ;

  int charset ;

  struct config_s m_conf ;

  char conf_path[PATH_MAX];

  struct backend_entry_s backend ;

  struct task_entry_s tasks ;

} g_mpInst = {

  .listenfd = -1,

  .max_sessions = 0,

  .status = SERVER_STATUS_AUTOCOMMIT,

  .charset= 0x21,

  .conf_path = "",
} ;


const struct __attribute__((__aligned__(64))) parse_pattern_s {
  char *ptns[3];
  int cmd_type ;
  int eof_count ; // total eof in responses
}
g_parse_ptns [] = {
  {{"select","@@version_comment",NULL,}, .cmd_type=s_sel_ver_comment, 2, },
  {{"select","DATABASE()",NULL,},        .cmd_type=s_sel_cur_db, 2, },

  {{"show","databases",NULL,},   .cmd_type=s_show_dbs, 2, },
  {{"show","tables",NULL,},      .cmd_type=s_show_tbls,2,  },
  {{"show","processlist",NULL,}, .cmd_type=s_show_proclst, 2, },

  {{"commit",NULL,NULL,},     .cmd_type=s_sharding, 0, },
  {{"rollback",NULL,NULL,},   .cmd_type=s_sharding, 0, },
  {{"set","autocommit",NULL,},.cmd_type=s_sharding, 0, },
  {{"select",NULL,NULL,},     .cmd_type=s_sharding, 2, },
  {{"update",NULL,NULL,},     .cmd_type=STMT_TYPE(s_sharding,s_single), 0, },
  {{"insert",NULL,NULL,},     .cmd_type=STMT_TYPE(s_sharding,s_single), 0, },
  {{"delete",NULL,NULL,},     .cmd_type=s_sharding, 0, },
  {{"create",NULL,NULL,},     .cmd_type=s_sharding, 0, },
  {{"drop",NULL,NULL,},       .cmd_type=s_sharding, 0, },
  {{"desc",NULL,NULL,},       .cmd_type=STMT_TYPE(s_sharding,s_single), 2, },
  {{"show",NULL,NULL,},       .cmd_type=STMT_TYPE(s_sharding,s_single), 2, },
  {{"alter",NULL,NULL,},      .cmd_type=s_sharding, 0, },
};



static int parse_cmd_line(int argc, char *argv[]);



ssize_t mp2_proto_rx(Network_t net, connection_t pconn)
{
  size_t total = dbuffer_data_size(pconn->rxb);
  dbuffer_t b = dbuffer_ptr(pconn->rxb,0);
  size_t szReq = 0L;

  /* mysql packet has header length==4 */
  if (total<=4) {
    return -1;
  }

  szReq = mysqls_get_req_size(b);
  /* not enough data for a mysql request */
  if (total<szReq) {
    return -1;
  }

  return szReq;
}

static
int do_com_init_db(Network_t net, connection_t pconn, char *inb, size_t sz)
{
  char db[128], *pwd = 0;
  session_t s = 0;
  size_t sz_out = 0;
  char outb[MAX_PAYLOAD];

  inb += 5 ;
  /* get db */
  memcpy(db,inb,sz-5);
  db[sz-5] = '\0';

  /* check new db by config */
  if (!is_db_exists(&g_mpInst.m_conf,db)) {
    log_error("no schema '%s'\n", db);
    sz_out = do_err_response(1,outb,ER_BAD_DB_ERROR,ER_BAD_DB_ERROR,db);
    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
    return -1;
  }

  /* get connection region */
  s = get_session(&g_mpInst.__sessions,pconn->fd);
  if (!s) {
    log_error("connetion id '%d' not found\n",pconn->fd);
    sz_out = do_err_response(1, outb, ER_ACCESS_DENIED_NO_PASSWORD_ERROR,
                             ER_ACCESS_DENIED_NO_PASSWORD_ERROR, "", db);
    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
    return -1;
  }

  /* get configure password */
  pwd = (char*)get_pwd(&g_mpInst.m_conf,db,s->usr);

  /* authenticate password */
  if (!pwd || mysqls_auth_usr(s->usr,pwd,s->pwd,strlen(s->pwd),s->scramble,s->sc_len)) {

    log_error("auth failed for user '%s'\n",s->usr);
    sz_out = do_err_response(1,outb,ER_ACCESS_DENIED_NO_PASSWORD_ERROR,
                             ER_ACCESS_DENIED_NO_PASSWORD_ERROR, s->usr, db);
    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
    return -1;
  }

  /* record new db */
#if 0
  s->db = write_dbuffer(s->db,db,strlen(db));
#else
  strncpy(s->db,db,sizeof(s->db));
#endif
  sz_out = do_ok_response(1,g_mpInst.status,outb);

  pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
  
  return 0;
}

static
int do_com_field_list(Network_t net, connection_t pconn,char *inb, size_t sz)
{
  size_t sz_out=0;
  /* target table name */
  char *tbl = inb+5 ;
  session_t s = get_session(&g_mpInst.__sessions,pconn->fd);
  char **rows;
  char *outb = 0; 
  /* FIXME */
  dbuffer_t con = 0;

  if (!s) {
    log_error("FATAL: connetion id %d not found\n", pconn->fd);
    return -1;
  }

  /* TODO: send real table column details */
#if 0
  {
    const size_t numRows = td->columns.size();
    char *rows[numRows];
    tColDetails *cd = 0;
    size_t total = 0;
    safeTableDetailList::col_itr itr ;
    bool bStart = true;

    /* calc the packet size and assign column names */
    for (uint16_t i=0;i<numRows;i++) {
      /* get each columns */
      cd = m_tables.next_col((char*)pss->db.c_str(),(char*)tbl.c_str(),itr,bStart);
      if (!cd)
        break ;

      total += strlen(cd->col.name)+pss->db.length()+tbl.length()+100 ;

      rows[i] = cd->col.name ;

      if (bStart) bStart=false;
    }

    con = alloc_dbuffer(total) ;
  }
#else
  char *__rows[] = {(char*)"com_field_list dummy"};

  rows = __rows ;
  /* FIXME!! */
  con = alloc_dbuffer(MAX_PAYLOAD);
#endif

  outb   = dbuffer_ptr(con,1);

  sz_out = mysqls_gen_qry_field_resp(outb,g_mpInst.status,g_mpInst.charset,
                                     1,s->db,tbl,rows,1);
  pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);

  drop_dbuffer(con);

  return 0;
}

static
int do_com_quit(Network_t net, connection_t pconn,char *inb, size_t sz)
{
  return 0;
}

static
int do_com_stmt_prepare(Network_t net, connection_t pconn,char *inb, size_t sz)
{
  return 0;
}

static
int do_com_stmt_close(Network_t net, connection_t pconn,char *inb, size_t sz)
{
  return 0;
}

static
int do_com_stmt_execute(Network_t net, connection_t pconn,char *inb, size_t sz)
{
  return 0;
}

static
int do_com_stmt_send_long_data(Network_t net, connection_t pconn,char *inb, size_t sz)
{
  return 0;
}

static
int do_fast_parse(const char *stmt, const size_t len, int *ptn_pos)
{
  for (size_t i=0; i<ARRAY_SIZE(g_parse_ptns);i++) {

    size_t offs = 0L;
    char *p = NULL;

    for (int n=0; (p=g_parse_ptns[i].ptns[n]) && n<3 ;n++) {

      while (isspace(stmt[offs])) offs++ ;

      if (strncasecmp(stmt+offs,p,strlen(p)))
        break ;

      offs += strlen(p);
    }

    if (!p) {

      if (g_parse_ptns[i].cmd_type==s_show_proclst && (offs+1)<len)
        continue ;

      *ptn_pos = i ;
      //log_debug("ptns: %s %s %s\n",g_parse_ptns[i].ptns[0],g_parse_ptns[i].ptns[1],g_parse_ptns[i].ptns[2]);

      return 0 ;
    }

  }

  /* invalid commands */
  return -1;
}

static
int do_show_dbs(connection_t pconn)
{
  size_t sz_out = 0, sz_total = 100;
  const size_t ndbs = get_num_schemas(&g_mpInst.m_conf);
  char *rows[ndbs];
  schema_t sc = 0;
  size_t i=0L;

  list_for_each_entry(sc,&g_mpInst.m_conf.m_schemas,upper) {
    rows[i++] = sc->name;
    sz_total += strlen(sc->name)+10;
  }

  {
    /* allocates out buffer */
    dbuffer_t buf = alloc_dbuffer(sz_total);
    char *outb = dbuffer_ptr(buf,1);
    char *cols[] = {(char*)"DataBase"} ;

    sz_out = mysqls_gen_normal_resp(outb,g_mpInst.status,g_mpInst.charset,
                                    1,(char*)"",(char*)"",cols,1,rows,ndbs);

    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
    drop_dbuffer(buf);
  }

  return 0;
}

static
int do_show_tbls(connection_t pconn)
{
  /* get connection region */
  session_t s = get_session(&g_mpInst.__sessions,pconn->fd);

  if (!s) {
    log_error("FATAL: connetion id %d not found\n",pconn->fd);
    return -1;
  }


  size_t i=0, sz_out = 0, sz_total = 150;
  schema_t sc = get_schema(&g_mpInst.m_conf,s->db);
  const size_t num_tbls = get_num_tables(sc);
  char *rows[num_tbls];
  table_info_t pt = 0;

  /* encoding the table name list */
  list_for_each_entry(pt,&sc->table_list,upper) {

    /* TODO: show valid tables only */
#if 0
    if (!m_tables.is_valid(td)) {
      valid_tbls--;
      continue ;
    }
#endif

    rows[i++] = pt->name;
    sz_total += strlen(pt->name) + 5;
  }

  {
    /* allocate output buffer */
    dbuffer_t buff = alloc_dbuffer(sz_total);
    char *outb = dbuffer_ptr(buff,1) ;
    char *cols[] = {(char*)"Table"} ;

    sz_out = mysqls_gen_normal_resp(outb,g_mpInst.status,g_mpInst.charset,
                                    1,(char*)"",(char*)"",cols,1,rows,num_tbls);

    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
    drop_dbuffer(buff);
  }

  return 0;
}

static
int do_sel_ver_comment(connection_t pconn)
{
  char outb[MAX_PAYLOAD] ;
  size_t sz_out = 0;
  char *rows[1] = { (char*)"mp2 source" } ;
  char *cols[1] = { (char*)"@@version_comment"}  ;

  sz_out = mysqls_gen_normal_resp(outb,g_mpInst.status,
    g_mpInst.charset,1,(char*)"",(char*)"",cols,1,rows,1);
  pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);

  return 0;
}

static 
int do_sel_cur_db(connection_t pconn)
{
  /* get connection region */
  session_t s = get_session(&g_mpInst.__sessions,pconn->fd);
  char outb[MAX_PAYLOAD] ;
  size_t sz_out = 0;

  if (!s) {
    log_error("FATAL: connetion id %d not found\n", pconn->fd);
    return -1;
  }

  {
    char *cols[1] = { (char*)"DATABASE()"};
    char *rows[1] = { (char*)s->db };

    sz_out = mysqls_gen_normal_resp(outb,g_mpInst.status,g_mpInst.charset,
                                    1,(char*)"",(char*)"",cols,1,rows,1);
    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
  }

  return 0;
}

static
int do_show_proclst(connection_t pconn)
{
  char *outb = 0 ;
  size_t total = 400, sz_out = 0;
  const size_t nCols = 9;
  const size_t nConn = get_num_sessions(&g_mpInst.__sessions);
  char *rows[nConn*nCols] ;
  session_t pos = 0, n=0;
  size_t i=0;
  const char proc[4] =" ";


  /* FIXME: we need mutex lock here!! */
  rbtree_postorder_for_each_entry_safe(pos,n,&g_mpInst.__sessions.u.root,node) {

    total += strlen(pos->db) + strlen(pos->usr) + 50 ;

    /* session id */
    rows[i*nCols+0] = (char*)pos->cmd.id;
    /* user */
    rows[i*nCols+1] = (char*)pos->usr;
    /* host */
    rows[i*nCols+2] = (char*)pos->addr;
    /* db */
    rows[i*nCols+3] = (char*)pos->db;
    /* command */
    rows[i*nCols+4] = (char*)pos->cmd.desc;
    /* time */
    char tmp[48] = "";
    snprintf(tmp,48,"%lld",time(NULL)-pos->cmd.times);
    strcpy(pos->cmd.s_times, tmp) ;
    rows[i*nCols+5] = (char*)pos->cmd.s_times;
    /* state */
    rows[i*nCols+6] = (char*)pos->cmd.stat;
    /* info */
    rows[i*nCols+7] = (char*)pos->cmd.info;
    /* progress */
    rows[i*nCols+8] = (char*)proc;

    i++ ;
  }

  {
    const char *cols[] = {"Id", "User", "Host", "Db",
      "Command", "Time", "State", "Info", "Progress"
    } ;

    dbuffer_t con = alloc_dbuffer(total) ;
    outb = dbuffer_ptr(con,1);

    sz_out = mysqls_gen_normal_resp(outb,g_mpInst.status,g_mpInst.charset,
                                    1,(char*)"",(char*)"",(char**)cols,nCols,rows,nConn);

    if (sz_out>=total) {
      log_error("fatal: much too data sz_out: %zu, total %zu!!\n",sz_out,total);
    }

    pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);
    drop_dbuffer(con);
  }

  return 0;
}

static
int do_com_query(Network_t net, connection_t pconn, char *inb, size_t sz)
{
  /* extract serial number */
  int ret = 0;
  char *pStmt = inb+5;
  int cmd = s_na ;
  const size_t szStmt = sz-5;
  int ptn_pos = -1;


  ret = do_fast_parse(pStmt,szStmt,&ptn_pos);

  if (!ret) {
    cmd = g_parse_ptns[ptn_pos].cmd_type ;
  }

  switch (GET_STMT_TYPE(cmd)) {
    /* make this com_query request be sharding in backends */
    case s_sharding:
      {
        ret = mp2_backend_com_query(net,pconn,inb,sz,ptn_pos);
      }
      break ;
    /* 'select DATABASE()' */
    case s_sel_cur_db:
      {
        ret = do_sel_cur_db(pconn);
      }
      break ;
    /* 'select @@version_comment' */
    case s_sel_ver_comment:
      {
        ret = do_sel_ver_comment(pconn);
      }
      break ;
    /* 'show databases' */
    case s_show_dbs:
      {
        ret = do_show_dbs(pconn);
      }
      break ;
    /* 'show tables' */
    case s_show_tbls:
      {
        ret = do_show_tbls(pconn);
      }
      break ;
    /* 'show tables' */
    case s_show_proclst:
      {
        ret = do_show_proclst(pconn);
      }
      break ;

    default:
      {
        size_t sz_out = 0;
        char outb[MAX_PAYLOAD] = "";

        log_error("unsupport query command : %s\n", pStmt);

        /* allocate response buffer */
        sz_out = do_err_response(0,outb,ER_INTERNAL_UNSUPPORT_SQL,
                                 ER_INTERNAL_UNSUPPORT_SQL, pStmt);

        pconn->txb = append_dbuffer(pconn->txb,outb,sz_out);

        ret = -1;
      }
      break ;
  }

  /* mark down the activities */
  set_session_last_command(&g_mpInst.__sessions, pconn->fd,
                           !ret?st_qry:st_error,pStmt,szStmt);

  return ret;
}

static
int do_com_login(Network_t net, connection_t pconn, char *inb,size_t sz)
{
  session_t s = 0;
  char usr[MAX_NAME_LEN]="",pwd_in[MAX_PWD_LEN]="",
    db[MAX_NAME_LEN]="", *pwd = 0;
  size_t sz_in = 0, sz_out=0;
  uint32_t sn = 0;
  int ret = 0;

  
  /* get serial number */
  sn = mysqls_extract_sn(inb);
  /* starts from packet body */
  inb = mysqls_get_body(inb);

  pconn->txb = realloc_dbuffer(pconn->txb,MAX_PAYLOAD);

  s = get_session(&g_mpInst.__sessions,pconn->fd) ;
  if (!s) {
    log_error("failed get connection info by id %d\n", pconn->fd);
    sz_out = do_err_response(sn,pconn->txb,ER_NET_READ_ERROR_FROM_PIPE,
                             ER_NET_READ_ERROR_FROM_PIPE);
    ret = -1 ;
    goto __end_login ;
  }

  /* check if connection is in 'init' state */
  if (s->status) {
    log_info("login session %d already exists\n",pconn->fd);
    reset_session(s);
  }

  if (mysqls_parse_login_req(inb,sz,usr,pwd_in,&sz_in,db)) {
    log_error("failed parsing login request by id %d\n", pconn->fd);
    sz_out = do_err_response(sn,pconn->txb, ER_NET_READ_ERROR_FROM_PIPE, 
                             ER_NET_READ_ERROR_FROM_PIPE);
    ret = -1 ;
    goto __end_login ;
  }

  if (*db!='\0' && !is_db_exists(&g_mpInst.m_conf,db)) {
    log_error("no database %s\n", db);
    sz_out = do_err_response(sn,pconn->txb,ER_BAD_DB_ERROR,ER_BAD_DB_ERROR,db);
    ret = -1 ;
    goto __end_login ;
  }

  pwd = (char*)get_pwd(&g_mpInst.m_conf,db,usr);
  if (!pwd) {
    log_error("no auth entry found for '%s'@'%s'\n", usr,db);
    sz_out = do_err_response(sn,pconn->txb,ER_ACCESS_DENIED_NO_PASSWORD_ERROR,
                             ER_ACCESS_DENIED_NO_PASSWORD_ERROR, usr, db);
    ret = -1 ;
    goto __end_login ;
  }

  /* authenticate password */
  if (mysqls_auth_usr(usr,pwd,pwd_in,sz_in,s->scramble,s->sc_len)) {
    log_error("auth failed for user %s\n",usr);
    sz_out = do_err_response(sn,pconn->txb, ER_ACCESS_DENIED_NO_PASSWORD_ERROR,
                             ER_ACCESS_DENIED_NO_PASSWORD_ERROR, usr, db);
    ret = -1 ;
    goto __end_login ;
  }

  update_session(s,db,usr,pwd_in,pconn->fd);

  log_debug("(%d) user %s@%s login ok\n",pconn->fd,usr,db);

  /* send an ok response */
  sz_out = do_ok_response(sn,g_mpInst.status,pconn->txb);

__end_login:
  dbuffer_lseek(pconn->txb,sz_out,SEEK_SET,1);

  return ret;
}

static
int default_com_handler(Network_t net, connection_t pconn, char *inb,size_t sz)
{
  char *cmd = 0;

  cmd = mysqls_get_body(inb);
  log_info("******* command %d\n",cmd[0]);
  return 0;
}

static 
void register_cmd_handlers(void)
{
  int i=0;

  /* standard mysql commands */
  for (;i<com_end;i++) {
    g_mpInst.m_handlers[i].ha = default_com_handler;
  }

  /* command 'com_query' */
  g_mpInst.m_handlers[com_query].ha = do_com_query;
  /* command 'com_init_db' */
  g_mpInst.m_handlers[com_init_db].ha = do_com_init_db;
  /* command 'com_field_list' */
  g_mpInst.m_handlers[com_field_list].ha = do_com_field_list;
  /* command 'com_quit' */
  g_mpInst.m_handlers[com_quit].ha = do_com_quit;
  /* command 'com_stmt_prepare' */
  g_mpInst.m_handlers[com_stmt_prepare].ha = do_com_stmt_prepare;
  /* command 'com_stmt_close' */
  g_mpInst.m_handlers[com_stmt_close].ha = do_com_stmt_close;
  /* command 'com_stmt_execute' */
  g_mpInst.m_handlers[com_stmt_execute].ha = do_com_stmt_execute;
  /* command 'com_stmt_send_long_data' */
  g_mpInst.m_handlers[com_stmt_send_long_data].ha = do_com_stmt_send_long_data;
}

int mp2_pre_init(int argc, char *argv[])
{
  char *host = 0;
  int port = -1;

  if (parse_cmd_line(argc,argv))
    return 1;

  if (init_config(&g_mpInst.m_conf,g_mpInst.conf_path)) {
    log_error("init config fail\n");
    return -1;
  }

  host = (char*)get_bind_address(&g_mpInst.m_conf);
  port = get_listen_port(&g_mpInst.m_conf);

  /* bind the listen address:port */
  g_mpInst.listenfd = new_tcp_svr(__net_atoi(host),port);

  if (g_mpInst.listenfd==-1) {
    log_error("init bind address fail\n");
    return -1;
  }

  g_mpInst.max_backend_tasks = get_max_backend_tasks(&g_mpInst.m_conf);

  g_mpInst.max_dn_groups     = get_dn_group_count(&g_mpInst.m_conf);

  g_mpInst.max_sessions      = ext_get_max_connections() ;

  log_info("done!\n");

  return 0;
}

void mp2_release()
{
  release_all_sessions(&g_mpInst.__sessions);

  free_config(&g_mpInst.m_conf);

  close(g_mpInst.listenfd);
}

int mp2_rx(Network_t net, connection_t pconn)
{
  ssize_t sz_in = 0L ;
  int cmd = 0;
  int rc = 0;


  while((sz_in = mp2_proto_rx(net,pconn))>0L && !rc) {

    dbuffer_t inb = dbuffer_ptr(pconn->rxb,0);

    /* process the inbound requests */
    if (!mysqls_is_packet_valid(inb,sz_in)) {
      log_error("invalid mysql packet received\n");
      goto __read_next;
    }
    /* set packet ending */
    inb[sz_in] = '\0';

    /* 
     * parse command code 
     */
    char *pb  = mysqls_get_body(inb);
    cmd = pb[0]&0xff ;

    if (mysqls_is_login_req(pb,sz_in)) {
      if (do_com_login(net,pconn,inb,sz_in)<0) 
        rc = -1;
    }
    else if (cmd>=com_sleep && cmd<com_end) {
      session_t s = get_session(&g_mpInst.__sessions,pconn->fd);

      if (!s) {
        log_error("found no session of fd %d!!!!\n",pconn->fd);
        goto __read_next ;
      }

      log_debug("cmd: %s chan %d\n",mysql_cmd2str[cmd],pconn->fd);

      if (g_mpInst.m_handlers[cmd].ha(net,pconn,inb,sz_in)<0) 
        rc = -1;
    }
    else 
      log_error("unknown command code %d\n",cmd);

__read_next:
    /* important: UPDATE THE READ POINTER OF RXB */
    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);
  }

  mp2_tx(net,pconn);

  return rc;
}

int mp2_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

  return 0;
}

int mp2_greeting(Network_t net, connection_t pconn)
{
  size_t sz = 0L;
  session_t s = 0;


  /* add an empty session */
  s = create_empty_session(&g_mpInst.__sessions,pconn->fd);
  if (!s) {
    log_error("add new ssesion for client %d fail\n",pconn->fd);
    return -1;
  }

  pconn->txb = realloc_dbuffer(pconn->txb,256);

  sz = mysqls_gen_greeting(pconn->fd,g_mpInst.charset,g_mpInst.status,
                           s->scramble,__VER_STR__,pconn->txb,256);

  /* update write pointer of tx buffer */
  dbuffer_lseek(pconn->txb,sz,SEEK_SET,1);

  log_debug("sending greeting to client %d\n",pconn->fd);

  return 0;
}

void mp2_close(Network_t net, connection_t pconn)
{
  session_t s = get_session(&g_mpInst.__sessions,pconn->fd);

  if (s) {
    log_debug("release session by fd %d\n",pconn->fd);
    drop_session(&g_mpInst.__sessions,s);
  }
}

/* 
 * backends 
 */
void mp2_backend_release()
{
  backend_t pos,n;


  /* close backend fds */
  for_each_backends(&g_mpInst.backend,pos,n) {
    close(pos->fd);
  }

  release_all_backends(&g_mpInst.backend);

  release_task_pool(&g_mpInst.tasks);
}

int mp2_backend_relogin(Network_t net, connection_t pconn)
{
  log_debug("done!\n");

  return 0;
}

int mp2_backend_login(Network_t net, connection_t pconn, backend_t bk)
{
  ssize_t ret = 0L ;


  pconn->txb = realloc_dbuffer(pconn->txb,MAX_PAYLOAD);

  ret = mysqlc_gen_login(&bk->prot,bk->usr,bk->pwd,bk->schema,0,
                         pconn->txb,MAX_PAYLOAD);
  if (ret<=0L) {
    log_error("gen login packet failed\n");
    return -1;
  }

  /* update tx pointer */
  dbuffer_lseek(pconn->txb,ret,SEEK_SET,1);

  mp2_backend_tx(net,pconn);

  /* state -> login */
  set_backend_state(bk,st_try_login);

  return 0;
}

int mp2_backend_do_task(Network_t net, task_t tsk, char *inb, size_t sz_in)
{
  int num_bk = tsk->backend_count ;
  backend_t *bk_array = (backend_t*)alloca(sizeof(backend_t)*num_bk);


  if (!inb) {
    get_task_req(tsk,&inb,&sz_in);
    if (!sz_in) {
      log_error("empty req size!!\n");
      return -1 ;
    }
  }

  for (int i=0;i<num_bk;i++) {

    int bknum = get_task_bk_num(tsk,i), in_use = 0;


    bk_array[i] = find_backend_by_num(&g_mpInst.backend,bknum,&in_use);

    if (!bk_array[i] && !in_use) {
      log_error("no backend '%d' found!\n",bknum);
      continue ;
    }

    if (in_use) {

      log_debug("backend %d 's NOT available, save to taskq, "
                "src fd %d\n",bknum,tsk->src->fd);

      save_task_req(tsk,inb,sz_in);

      task_ref_set(tsk,0);

      /* free all backends that already in-used  */
      while (--i>=0)
        set_backend_unuse(bk_array[i]);

      task_enqueue(&g_mpInst.tasks,tsk);

      return 1;
    }

    task_ref_add(tsk);

    set_backend_usage(bk_array[i],tsk);
  }

  for (size_t i=0;i<num_bk;i++) {

    if (!bk_array[i])
      continue ;

    connection_t bk_conn  = bk_array[i]->conn ;

    bk_conn->txb = append_dbuffer(bk_conn->txb,(char*)inb,sz_in);

    mp2_backend_tx(net,bk_conn);
  }

  return 0;
}

int mp2_backend_process_task(Network_t net)
{
  task_t tsk = task_dequeue(&g_mpInst.tasks);


  if (tsk) {
    log_debug("request is done, processing task in queue\n");

    mp2_backend_do_task(net,tsk,NULL,0L);
  }
  else {
    show_pool_item_count(&g_mpInst.tasks);
    show_backend_in_use(&g_mpInst.backend);
  }

  return 0;
}

static
int mp2_backend_get_route(connection_t pconn, int cmd, int bk_num_list[], int *num_bk)
{
  int no = 0;


  //log_debug("total backends %d\n",*num_bk);

  /* route to all */
  if (!(GET_OPT_BITS(cmd))) {
    get_all_backend_num(&g_mpInst.backend,bk_num_list,*num_bk);
    log_debug("route to all backends\n");
    return 0;
  }

  no = pconn->fd % *num_bk ;

  log_debug("route to backend no %d\n",no);

  bk_num_list[0] = no ;

  *num_bk = 1;

  return 0;
}

int mp2_backend_com_query(Network_t net, connection_t pconn, 
    const char *inb, const size_t sz, const int ptn_pos)
{
  task_t tsk = 0;
  int num_bk = get_backend_count(&g_mpInst.backend) ;
  int *bk_num_list = (int*)alloca(sizeof(int*)*num_bk);
  int cmd = g_parse_ptns[ptn_pos].cmd_type ;


  mp2_backend_get_route(pconn,cmd,bk_num_list,&num_bk);

  tsk = alloc_task(&g_mpInst.tasks,num_bk);
  if (!tsk) {
    size_t sz_out = 0L;

    pconn->txb = realloc_dbuffer(pconn->txb,MAX_PAYLOAD);
    sz_out = do_err_response(1,pconn->txb,ER_INTERNAL_NO_MORE_TASK,
                             ER_INTERNAL_NO_MORE_TASK);
    dbuffer_lseek(pconn->txb,sz_out,SEEK_SET,1);

    log_error("alloc new task fail\n");
    return -1;
  }

  tsk->src = pconn ;
  tsk->ptn_pos = ptn_pos ;


  for (size_t i=0;i<num_bk;i++) { 
    save_task_bk_num(tsk,i,bk_num_list[i]);
  }

  mp2_backend_do_task(net,tsk,(char*)inb,sz);

  return 0;
}

static
int mp2_backend_deal_res(Network_t net, connection_t pconn, char *inb, size_t sz_in)
{
  backend_t bk = get_backend(&g_mpInst.backend,pconn->fd);
  task_t tsk = 0;
  int send_it = 0;
  bool is_single_res = mysqls_is_error(inb,sz_in) || 
       mysqls_extract_column_count(inb,sz_in)==0 ;


  if (!bk) {
    log_error("no backend found by backend fd %d\n",pconn->fd);
    return -1;
  }

  tsk = bk->task ;
  if (unlikely(!tsk)) {
    log_error("no task found by backend fd %d\n",pconn->fd);
    return -1;
  }

  if (tsk->backend_count==1) {
    send_it = 1;
  } 
  else {
    if (bk->eof_count<1) {
      if (tsk->sn<mysqls_extract_sn(inb)) {
        send_it = 1;
        tsk->sn++;
      }
    }
    else if (bk->eof_count<2) {
      if (!mysqls_is_eof(inb,sz_in)) {
        mysqls_update_sn(inb,++tsk->sn);
        send_it = 1;
      }
    }
  }

  if ((mysqls_is_eof(inb,sz_in) && (++bk->eof_count == g_parse_ptns[tsk->ptn_pos].eof_count)) 
      || is_single_res) {

    set_backend_unuse(bk);
    log_debug("backend '%d' is freed111111\n",bk->no);

    if (--tsk->ref_count == 0) {
      free_task(&g_mpInst.tasks,tsk);

      /* do this ONLY IF the request is done */
      mp2_backend_process_task(net);

      /* send last eof */
      if (tsk->backend_count>1 && !is_single_res) {
        mysqls_update_sn(inb,++tsk->sn);
      }

      send_it = 2;
    }

  }


  if (send_it>0) {
    if (is_single_res)
      tsk->src->txb = write_dbuffer(tsk->src->txb,inb,sz_in);
    else
      tsk->src->txb = append_dbuffer(tsk->src->txb,inb,sz_in);
  }


  if (send_it==2 || dbuffer_data_size(tsk->src->txb)>MAX_PAYLOAD) {
    mp2_backend_tx(net,tsk->src);
  }

  return 0;
}

int mp2_backend_rx(Network_t net, connection_t pconn)
{
  ssize_t sz_in = 0L ;
  backend_t bk = get_backend(&g_mpInst.backend,pconn->fd);


  if (unlikely(!bk)) {
    log_error("found no backend info by fd %d\n",pconn->fd);
    return -1;
  }

  while((sz_in = mp2_proto_rx(net,pconn))>0L) {

    dbuffer_t inb = dbuffer_ptr(pconn->rxb,0);

    if (!mysqls_is_packet_valid(inb,sz_in)) {
      log_error("invalid mysql packet received\n");
      goto __read_next;
    }

    char *pbody  = mysqls_get_body(inb);
    size_t szbody = mysqls_get_body_size(inb);

    /* initial state, this's a greeting from backend server */
    if (bk->status==st_empty) {

      if (mysqlc_parse_greeting(&bk->prot,pbody,szbody)) {
        log_error("error parse greetings by fd %d\n",pconn->fd);
      } else {
        /* try login */
        mp2_backend_login(net,pconn,bk);
      }
    }
    /* login response */
    else if (bk->status==st_try_login) {

      int stat = !mysqls_parse_response(&bk->prot,pbody,szbody)?
                 st_login_ok:st_login_fail ;

      set_backend_state(bk,stat);

      if (stat==st_login_ok)
        bk->conn = pconn ;

      log_info("login to '%s:%d [%s]' %s\n",bk->addr,bk->port,
               bk->schema, bk->status==st_login_ok?"ok":"fail");
    }
    /* normal processing */
    else if (bk->status==st_login_ok) {

      mp2_backend_deal_res(net,pconn,inb,sz_in);
    }

__read_next:
    /* try to read the next packet if possible */
    dbuffer_lseek(pconn->rxb,sz_in,SEEK_CUR,0);
  }

  return 0;
}

int mp2_backend_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

  return 0;
}

void mp2_backend_close(Network_t net, connection_t pconn)
{
  drop_backend(&g_mpInst.backend,pconn->fd);

  log_debug("backend fd %d close\n",pconn->fd);
}


static
struct module_struct_s g_module = {

  .name = "mp2",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[inbound_l5] = {
    .init     = mp2_init,
    .release  = mp2_release,
    .rx       = mp2_rx,
    .tx       = mp2_tx,
    .greeting = mp2_greeting,
    .close    = mp2_close,
  },

  .opts[outbound_l5] = {
    .init    = mp2_backend_init,
    .release = mp2_backend_release,
    .rx      = mp2_backend_rx,
    .tx      = mp2_backend_tx,
    .close   = mp2_backend_close,
  },

};

static
int parse_cmd_line(int argc, char *argv[])
{
  char *path = 0;


  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-cm") && i+1<argc) {
      path = argv[i+1];
      strcpy(g_mpInst.conf_path,path);
      printf("conf: %s\n",path);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("module '%s' help message\n",THIS_MODULE->name);
      printf("-cm: configure file path\n");
      return 1;
    }
  }

  return 0;
}

int mp2_init(Network_t net)
{
  init_session_entry(&g_mpInst.__sessions,g_mpInst.max_sessions);

  register_cmd_handlers();

  /* listenfd -> event epoll */
  net->reg_local(net,g_mpInst.listenfd,THIS_MODULE->id);

  return 0;
}

int mp2_backend_init(Network_t net)
{
  backend_t pos,n;
  data_node_t dn =0 ;
  int fd = 0;


  init_task_pool(&g_mpInst.tasks,g_mpInst.max_backend_tasks);

  init_backend_entry(&g_mpInst.backend);

  for (int i=0;i<g_mpInst.max_dn_groups;i++) {

    list_for_each_entry(dn,&g_mpInst.m_conf.m_dataNodes,upper) {

      fd = new_tcp_socket();

      create_backend(&g_mpInst.backend, fd, dn->num, dn->address, dn->port,
                     dn->schema, dn->auth.usr, dn->auth.pwd);

      log_debug("datanode %s(%d), addr: %s:%d, login: %s,%s, schema: %s\n",
                dn->name,dn->num,dn->address,dn->port,dn->auth.usr,dn->auth.pwd,dn->schema);
    }
  }

  /* backend fds -> event epoll */
  for_each_backends(&g_mpInst.backend,pos,n) {
    net->reg_outbound(net,pos->fd,THIS_MODULE->id);
  }

  /* connect to backend dbs */
  for_each_backends(&g_mpInst.backend,pos,n) {
    if (new_tcp_client2(pos->fd,__net_atoi(pos->addr),pos->port)) 
      log_error("connect to %s:%d fail!\n",pos->addr,pos->port);
  }

  return 0;
}

static 
void mp2_dump_params()
{
  char *host = (char*)get_bind_address(&g_mpInst.m_conf);
  int port = get_listen_port(&g_mpInst.m_conf);


  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  log_info("bound to: %s:%d\n",host,port);
  log_info("max_sessions: %zu\n",g_mpInst.max_sessions);
  log_info("max_backend_tasks: %zu\n",g_mpInst.max_backend_tasks);
  log_info("max_dn_groups: %zu\n",g_mpInst.max_dn_groups);
  log_info("module param list end =================\n");
}

void mp2_module_init(int argc, char *argv[])
{
  if (mp2_pre_init(argc,argv))
    return ;

  mp2_dump_params();

  register_module(THIS_MODULE);
}

//MODULE_INIT(mp2_module_init)

void mp2_module_exit()
{
  log_info("module '%s' exit\n",THIS_MODULE->name);

  mp2_release();
}

//MODULE_EXIT(mp2_module_exit)

