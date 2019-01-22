
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unistd.h>
#include "mysqls.h"
#include "crypto.h"
#include "mysql_porting.h"
#include "simple_types.h"
#include "log.h"


/* auth plugin data len 1 */
#define AUTH_PLUGIN_LEN1  8

/* the default auth plugin name */
#define DEFAULT_AP_NAME  "mysql_native_password"



void mysqls_gen_rand_string(char *s, size_t sz)
{
  char *end = s + sz ;

  srand((size_t)s*sz);
  for (;s<end;s++)
    *s = (char)rand()*33+94;
  *s = '\0';
}

/*
 * mysqls interfaces
 */

/* check packet validation */
bool mysqls_is_packet_valid(char *pkt, size_t sz)
{
  return byte3_2_ul(pkt)<=(sz-4) ;
}

bool mysqls_is_eof(char *pkt, size_t sz)
{
  return (pkt[4]&0xff)==0xfe ;
}

/* generates the server greeting packet */
int mysqls_gen_greeting(uint32_t conn_id,
                        int charset, uint32_t svr_stat, 
                        char *scramble, char *ver, 
                        char *outb, size_t sz)
{
  char *begin = outb, *psz = outb ;
  uint32_t cflags = 0;
  int ap_len = 0, rest = 0;

  /* the packet header */
  outb+=3 ;
  /* serial number */
  outb[0] =0;
  outb++ ;
  /* capability flags */
  cflags = CLIENT_BASIC_FLAGS;
  /* protocol version, always 0xa */
  outb[0] = 0xa ;
  /* version string */
  strcpy(&outb[1],ver);
  /* fill connection id */
  outb = strend(outb+1)+1;
  ul4store(conn_id,outb);
  outb+=4 ;
  /* the plugin data part 1 */
  memcpy(outb,scramble,AUTH_PLUGIN_LEN1);
  outb += AUTH_PLUGIN_LEN1 ;
  /* filter */
  outb[0] = 0;
  outb++;
  /* capability lower part */
  ul2store(cflags,outb);
  outb+=2 ;
  /* character set number */
  outb[0] = (char)charset ;
  outb++ ;
  /* server status */
  ul2store(svr_stat,outb);
  outb+=2 ;
  /* capability upper part */
  ul2store(cflags>>16,outb);
  outb+=2 ;
  /* auth plugin data length, including the 
   *  null terminate char */
  ap_len = outb[0] = AP_LENGTH ;
  outb++ ;
  /* 10 reserve '0' */
  bzero(outb,10);
  outb+=10;
  rest = ap_len - AUTH_PLUGIN_LEN1 -1 ;
  /* rest part of scramble */
  memcpy(outb,scramble+AUTH_PLUGIN_LEN1,rest);
  outb[rest] = 0;
  outb += rest+1 ;
  /* the auth plugin name, only 'prot_native_password' */
  rest = sizeof(DEFAULT_AP_NAME);
  memcpy(outb,DEFAULT_AP_NAME,rest);
  outb[rest] = 0;
  outb += rest ;
  /* write back the packet size */
  ul3store(outb-begin-4,psz);
  return outb-begin;
}

#if 0
bool mysqls_is_login_req(char *inb,size_t sz)
{
  /* check if it's login request packet */
  char *p = inb+9 ;

  if (sz<(9+23))
    return false;
  /* not 23 zero, not a login request */
  for (;!*p&&p<(inb+9+23);p++);
  if (p<(inb+9+23)) {
    return false ;
  }
  return true ;
}
#else
bool mysqls_is_login_req(char *inb,size_t sz)
{
  /* check if it's login request packet */
  const char ch23zeros[23] = 
  {
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,
  };

  if (sz<(9+23))
    return false;
  return !memcmp(inb+9,ch23zeros,23) ;
}
#endif

int mysqls_parse_login_req(char *inb,size_t sz, char *usr, 
                           char *pwd, size_t *sz_pwd, char *db)
{
  char *end = inb ;
  uint32_t caps = 0, i=0;
  size_t ln =0;

  /* the client capabilites */
  caps = byte4_2_ul(end);
  end += 4 ;
  /* max packet size */
  end += 4 ;
  /* the char set */
  // end[0] == pv->char_set
  end ++ ;
  /* skip reserve '0' s */
  end += 23;
  /* user name */
  ln = strlen(end);
  ln = ln<MAX_NAME_LEN?ln:MAX_NAME_LEN;
  strncpy(usr,end,ln);
  usr[ln] = '\0';
  end += ln+1;
  /* authenticate data  */
  if (caps&client_secure_connection) {
    ln = end[0] ;
    end++ ;
  } else if (caps&client_plugin_auth_lenenc_client_data) {
    ln = lenenc_int_get(&end);
  } else {
    ln = strlen(end)+1;
  }
  memcpy(pwd,end,ln);
  pwd[ln] = '\0';
  end += ln ;
  *sz_pwd = ln ;
  /* db selection */
  if (caps&client_connect_with_db) {
    ln = strlen(end);
    ln = ln<MAX_NAME_LEN?ln:MAX_NAME_LEN;
    strncpy(db,end,ln);
    db[ln] = '\0';
    end += ln+1;
  }
  /* plugin auth name, just skip it now because we
   *  have only one auth plugin method */
  if (caps&client_plugin_auth) {
    ln = strlen(end)+1;
    end += ln ;
  }
  /* skip connection attributes now */
  if (caps&client_connect_attrs) {
    /* number of the attribute pairs */
    ln = lenenc_int_get(&end);
    for (i=0;i<ln;) {
      int len = 0;

      /* skip 'key' */
      len = lenenc_int_get(&end);
      //end += len ;
      /* try value bytes */
      i += len ;
      /* skip 'value' */
      len = lenenc_int_get(&end);
      //end += len ;
      /* try next key */
      i += len ;
    }
  }
  return 0;
}

int mysqls_auth_usr(char *usr,char *orig_pwd,
                    char *in_pwd,size_t in_len,
                    char *scramble,size_t sc_len)
{
  uint8_t enc_pwd[MAX_PWD_LEN];

  /* encrypt the password */
  if (gen_auth_data(scramble,sc_len,orig_pwd,enc_pwd)) {
    log_debug("get auth data fail\n");
    return -1;
  }
#if 0
  /* XXX: test */
  {
    log_error("lens: %zu, %zu, %zu\n",sc_len,strlen((char*)enc_pwd),strlen((char*)in_pwd));
    log_error("%s: enc pwd\n",__func__);
    hex_dump((char*)enc_pwd,strlen((char*)enc_pwd));
    log_error("%s: in pwd(%zu)\n",__func__,strlen(in_pwd));
    hex_dump((char*)in_pwd,strlen((char*)in_pwd));
    log_error("%s: scramble(%zu)\n",__func__,sc_len);
    hex_dump((char*)scramble,sc_len);
  }
#endif
  /* compare with inputed password */
  if (memcmp(in_pwd,enc_pwd,sc_len)) {
    log_debug("cmp password fail\n");
    return -1;
  }
  return 0;
}

size_t mysqls_gen_rollback(char *inb)
{
  char *psz = inb, *end = inb;

  /* skip size bytes */
  end += 3;
  /* serial number */
  end[0] = 0 ;
  end++;
  /* command code */
  end[0] = com_query ;
  end++ ;
  /* query string */
  end = stpcpy(end,"rollback");
  /* packet size */
  ul3store(end-inb-4,psz);
  return end-inb;
}

int mysqls_gen_ok(char *inb, uint16_t st,uint32_t sn, 
                  uint64_t affected_rows, uint64_t last_id)
{
  char *psz = inb, *end = inb ;

  /* the packet size will set later */
  end += 3; 
  /* the packet sn */
  end[0] = sn ;
  end ++;
  /* OK header */
  end[0] = 0;
  end ++;
  /* affected rows */
  end += lenenc_int_set(affected_rows,end);
#if 0
  end[0] = 0;
  end ++;
#endif
  /* last insert id */
  end += lenenc_int_set(last_id,end);
#if 0
  end[0] = 0;
  end ++;
#endif
  /* server status */
  ul2store(st,end);
  end += 2;
  /* warnings */
  ul2store(0,end);
  end += 2;
  /* TODO: warning messages here if warnings>0 */
  /* set packet size */
  ul3store(end-inb-4,psz);
  return end-inb;
}

int mysqls_sprintf(char *outb, int err, va_list args)
{
  char *fmt = find_mysqld_error(err),
    *ptr = fmt, *prev = 0;
  const size_t flen= strlen(fmt);
  char fb[128]="",fmb[128]="";
  bool bFmt = false ;

  if (!fmt) {
    return -1;
  }
  /* process format arguments */
  for (;ptr<(fmt+flen);ptr++) {
    /* a format pattern begins */
    if (*ptr=='%') {
      bFmt = true ;
      prev = ptr ;
    } else if (!bFmt) {
      *outb++ = *ptr ;
    } else if (*ptr=='s' || *ptr=='d' || *ptr=='u') {
      /* copy the pattern */
      memcpy(fmb,prev,ptr-prev+1);
      fmb[ptr-prev+1] = '\0';
      /* instantiates the format pattern */
      if (*ptr=='s') {
        sprintf(fb,fmb,(char*)va_arg(args,char*));
      }
      if (*ptr=='d') {
        if (!strncmp(ptr-2,"ll",2))
          sprintf(fb,fmb,(long long)va_arg(args,long long));
        else if (*(ptr-1)=='l')
          sprintf(fb,fmb,(long)va_arg(args,long));
        else 
          sprintf(fb,fmb,(int)va_arg(args,int));
      }
      if (*ptr=='u') {
        if (!strncmp(ptr-2,"ll",2))
          sprintf(fb,fmb,(uint64_t)va_arg(args,uint64_t));
        else if (*(ptr-1)=='l')
          sprintf(fb,fmb,(uint32_t)va_arg(args,uint32_t));
        else 
          sprintf(fb,fmb,(unsigned int)va_arg(args,unsigned int));
      }
      /* concats the pattern to out buffer */
      strcpy(outb,fb);
      outb += strlen(fb);
      bFmt = false ;
    } 
  } /* for (;) */
  *outb = '\0';
  return 0;
}

uint8_t mysqls_is_error(char *inb,size_t sz)
{
  return sz<5?true:(inb[4]&0xff)==0xff;
}

static int 
mysqls_gen_err(char *inb, int err_code,uint32_t sn)
{
  char *end = inb ;

  /* the packet size will set later */
  end += 3; 
  /* the packet sn */
  end[0] = sn ;
  end ++;
  /* error header */
  end[0] = 0xff;
  end ++ ;
  /* the error code */
  ul2store(err_code,end);
  end+=2;
  /* sql state marker */
  end[0] = '#';
  end++ ;
  return end-inb;
}

int mysqls_gen_error(char *inb, uint16_t sql_stat, 
                     int err_code, uint32_t sn, va_list args)
{
  char *psz = inb, *end = inb ;
  char *str=0;

#if 0
  /* the packet size will set later */
  end += 3; 
  /* the packet sn */
  end[0] = sn ;
  end ++;
  /* error header */
  end[0] = 0xff;
  end ++ ;
  /* the error code */
  ul2store(err_code,end);
  end+=2;
  /* sql state marker */
  end[0] = '#';
  end++ ;
#else
  end += mysqls_gen_err(inb,err_code,sn);
#endif
  /* the sql state string */
  str = find_sql_state(sql_stat);
  if (str) memcpy(end,str,5);
  else bzero(end,5);
  end += 5;
  /* the error string */
  mysqls_sprintf(end,err_code,args);
  end = strend(end)+1;
  /* set packet size */
  ul3store(end-inb-4,psz);
  return end-inb;
}

int do_ok_response(uint32_t sn, int stat, char outb[/*MAX_PAYLOAD*/])
{
  size_t size = 0;

  if ((size=mysqls_gen_ok(outb,stat,sn+1,0L,0L))<0) {
    return -1;
  }
  return size;
}

int 
do_err_response(uint32_t sn, char outb[/*MAX_PAYLOAD*/], 
                int state, int err, ...)
{
  size_t size = 0;
  va_list args ;

  va_start(args,err);
  size = mysqls_gen_error(outb,state,err,sn+1,args) ;
  va_end(args);
  if (size<0) {
    return -1;
  }
  return size;
}

static int 
mysqls_gen_eof(char *inb, int sn,uint16_t warn, uint16_t stat)
{
  char *psz = inb, *end = inb;

  /* skip size bytes */
  end += 3;
  /* serial number */
  end[0] = sn ;
  end++;
  /* marker */
  end[0] = 0xfe ;
  end++ ;
  /* warnings */
  ul2store(warn,end);
  end+=2;
  /* status */
  ul2store(stat,end);
  end+=2;
  /* packet size */
  ul3store(end-inb-4,psz);
  return end-inb;
}

static int 
mysqls_gen_col_def(char *inb, int sn, char *db, 
                   char *tbl, char *tbl_a, char *col, 
                   char *col_a, int char_set, uint32_t max_len, 
                   int field_t, uint16_t col_flags, char *def)
{
  char *end = inb, *psz = inb ;

  /* SKIP packet size */
  end+=3;
  /* fill in serial number */
  end[0] = sn ;
  end++ ;
  /* 'catalog' */
  end += lenenc_str_set(end,(char*)"def");
  /* 'database' */
  end += lenenc_str_set(end,db);
  /* 'table' */
  end += lenenc_str_set(end,tbl);
  /* 'original table' */
  end += lenenc_str_set(end,tbl_a);
  /* 'name' */
  end += lenenc_str_set(end,col);
  /* 'original name' */
  end += lenenc_str_set(end,col_a);
  /* next length */
  end[0] = 0xc ;
  end++ ;
  /* char set */
  ul2store(char_set,end);
  end+=2;
  /* max column length */
  ul4store(max_len,end);
  end+=4 ;
  /* field type */
  end[0] = field_t ;
  end++ ;
  /* column flags */
  ul2store(col_flags,end);
  end+=2;
  /* decimal */
  end[0] = 0;
  end++;
  /* filters */
  ul2store(0,end);
  end+=2;
  /* the 'default' field */
  if (def) {
    int ln = strlen(def);

    end += lenenc_int_set(ln,end);
    memcpy(end,def,ln);
    end += ln ;
  }
  /* write back packet size */
  ul3store(end-inb-4,psz);
  return end-inb ;
}

int mysqls_extract_sn(char *inb)
{
  return inb[3]&0xff;
}

void mysqls_update_sn(char *inb, uint8_t sn)
{
  inb[3] = sn ;
}

uint8_t mysqls_get_cmd(char *inb)
{
  return inb[4];
}

void mysqls_set_cmd(char *inb, uint8_t cmd)
{
  inb[4] = cmd;
}

char* mysqls_get_body(char *inb)
{
  return inb+4;
}

size_t mysqls_get_body_size(char *inb)
{
  return byte3_2_ul(inb);
}

size_t mysqls_get_req_size(char *inb)
{
  return byte3_2_ul(inb)+4;
}

void mysqls_update_req_size(char *inb, size_t sz)
{
  ul3store(sz,inb);
}

int 
mysqls_gen_qry_field_resp(char *inb, uint16_t sql_stat, 
                          int char_set,uint32_t sn, char *db, 
                          char *tbl, char **rows, size_t numRows)
{
  char *end = inb ;
  uint16_t i=0;

  /* 
   * #1 column definitions
   */
  for (i=0;i<numRows;i++) {
    end += mysqls_gen_col_def(end,sn++,db,
      tbl,(char*)"",rows[i],(char*)"",
      char_set,64,MYSQL_TYPE_VAR_STRING,
      NOT_NULL_FLAG,(char*)"0");
  }
  /* 
   * #2 eof packet
   */
  end += mysqls_gen_eof(end,sn++,0,sql_stat);
  return end-inb;
}

ssize_t 
mysqls_gen_normal_resp(char *inb, uint16_t sql_stat, int char_set, 
                       uint32_t sn, char *db, char *tbl, char **cols, 
                       size_t num_cols, char **rows, size_t num_rows)
{
  uint16_t i=0;
  char *psz = inb, *end = inb, *begin = inb ;

  /*
   * #1 field count
   */
  /* packet size */
  ul3store(1,end);
  end+=3;
  /* fill in serial number */
  end[0] = sn++ ;
  end++ ;
  /* column count */
  end[0] = num_cols;
  end++;
  /* 
   * #2 column definitions
   */
  for (i=0;i<num_cols;i++) {
    end += mysqls_gen_col_def(end,sn++,db,
      tbl,(char*)"",(char*)cols[i],
      (char*)"",char_set,64,MYSQL_TYPE_VAR_STRING,
      NOT_NULL_FLAG,NULL);
  }
  /* 
   * #3 eof packet
   */
  end += mysqls_gen_eof(end,sn++,0,sql_stat);
  /*
   * #4 encoding rows
   */
  for (i=0;i<num_rows;i++) {
    inb = end ;
    /* SKIP packet size */
    psz = inb ;
    end+=3;
    /* fill in serial number */
    end[0] = sn++ ;
    end++ ;

    /* row contents by columns */
    for (int n=0;n<num_cols;n++) {
      end += lenenc_str_set(end,rows[i*num_cols+n]);
    }

    /* write back packet #2 size */
    ul3store(end-inb-4,psz);
  }
  /*
   * #5 eof packet
   */
  end += mysqls_gen_eof(end,sn++,0,sql_stat);
  return end-begin;
}

bool mysqls_is_digit_type(uint8_t type)
{
  return type==MYSQL_TYPE_TINY ||
		type==MYSQL_TYPE_SHORT     ||
    type==MYSQL_TYPE_LONG      ||
		type==MYSQL_TYPE_FLOAT     ||
    type==MYSQL_TYPE_DOUBLE    ||
    type==MYSQL_TYPE_LONGLONG  ||
    type==MYSQL_TYPE_INT24 ;
}

int mysqls_get_col_full_name(void *cols, int ncol, char **db,
                             char **tbl, char **col, uint8_t *type)
{
  MYSQL_COLUMN *pc = (MYSQL_COLUMN*)cols+ncol ;

  if (db)  *db   = pc->schema ;
  if (tbl) *tbl  = pc->tbl ;
  if (col) *col  = pc->name ;
  if (type)*type = pc->type;
  return 0;
}

char* mysqls_save_one_col_def(void *p, char *ptr, char *def_db)
{
  MYSQL_COLUMN *pc = p ;

  /* skip length + sn bytes */
  ptr+= 4;
  /* skip catalog */
  ptr = lenenc_str_get(ptr,0,MAX_NAME_LEN);
  /* schema */
  ptr = lenenc_str_get(ptr,pc->schema,MAX_NAME_LEN);
  if (def_db) strncpy(pc->schema,def_db,MAX_NAME_LEN);
  /* owner table alias */
  ptr = lenenc_str_get(ptr,pc->tbl_alias,MAX_NAME_LEN);
  /* owner table */
  ptr = lenenc_str_get(ptr,pc->tbl,MAX_NAME_LEN);
  /* column alias */
  ptr = lenenc_str_get(ptr,pc->alias,MAX_NAME_LEN);
  /* column name */
  ptr = lenenc_str_get(ptr,pc->name,MAX_NAME_LEN);
  /* skip the 'next length' 0xc */
  ptr++ ;
  /* column character set */
  pc->charset = byte2_2_ul(ptr);
  ptr+=2 ;
  /* max length of column */
  pc->len = byte4_2_ul(ptr);
  ptr+=4 ;
  /* XXX: get column type, this's important */
  pc->type= ptr[0] ;
  ptr++ ;
  /* column flags */
  pc->flags = byte2_2_ul(ptr);
  ptr+=2 ;
  /* max shown digit */
  pc->decimal = ptr[0] ;
  ptr++ ;
  /* skip filter bytes */
  ptr+=2;
  /*log_error("%s: col %d, schema %s, tbl %s:%s, "
    "col %s:%s, type %d\n", __func__,nCol, pc->schema,
    pc->tbl, pc->tbl_alias, pc->name,pc->alias,pc->type);*/
  return ptr ;
}

int 
mysqls_extract_prepared_info(char *inb, size_t sz, int *stmtid,
                             int *nCol, /* column count */
                             int *nPh  /* place holder count */)
{
  /* should not less than 5 bytes */
  if (sz<16) {
    return -1;
  }
  /* skip length bytes */
  inb += 3;
  /* skip sn byte */
  inb ++;
  /*skip err byte */
  inb ++ ;
  /* the statement id */
  if (stmtid) 
    *stmtid = byte4_2_ul(inb);
  inb += 4;
  /* column count */
  if (nCol)
    *nCol = byte2_2_ul(inb);
  inb += 2;
  /* placeholder count */
  if (nPh)
    *nPh = byte2_2_ul(inb);
  inb += 2;
  return 0;
}

int mysqls_extract_column_count(char *inb, size_t sz)
{
  /* should not less than 5 bytes */
  if (sz<5) {
    return -1;
  }
  /* skip length bytes */
  inb += 3;
  /* skip sn byte */
  inb ++;
  /* column count */
  return inb[0]&0xff;
}

int mysqls_update_stmt_prep_stmt_id(char *inb, size_t sz, int id)
{
  char *end = inb ;

  if (sz<10) {
    return -1;
  }
  ul4store(id,end+5);
  return 0;
}

int mysqls_get_stmt_prep_stmt_id(char *inb, size_t sz, int *id)
{
  char *end = inb ;

  if (sz</*10*/9) {
    return -1;
  }
  *id = byte4_2_ul(end+5);
  return 0;
}

int mysqls_gen_stmt_close(char *inb, int stmtid)
{
  char *psz = inb, *end = inb;

  /* skip size bytes */
  end += 3;
  /* serial number */
  end[0] = 0 ;
  end++;
  /* command */
  end[0] = com_stmt_close;
  end++;
  /* statement id */
  ul4store(stmtid,end);
  end+=4;
  /* packet size */
  ul3store(end-inb-4,psz);
  return end-inb;
}

int mysqlc_parse_greeting(mysqlclient_proto_t prot, char *inb, size_t sz_in)
{
  char *ptr=inb, *end= inb+sz_in, *tmp=0;
  /* auth plugin data */
  char *ap_data = 0;
  /* the length of 1st 'auth data' */
  const int AP_DATA1_LEN = 8,
    /* max length of 2nd 'auth data' */
    AP_DATA2_LEN = 12 ;
  const char DEFAULT_PLUGIN_NAME[] = "mysql_native_password";
  /* standard protocol version */
  const int proto_ver = 0x0A ;
  /* server capabilyties */
  enum svr_caps {
    /* Client supports plugin authentication */
    sc_client_auth_plugin = (1UL << 19),
  } ;


  /* protocol version */
  prot->proto_version = ptr[0] ;
  if (prot->proto_version != proto_ver) {
    log_error("invalid protocol version 0x%x\n", prot->proto_version);
    return -1;
  }
  ptr++ ;

  /* server version */
  tmp = stpcpy(prot->svr_version,ptr);
  log_debug("server version: %s\n", prot->svr_version);
  ptr+= (tmp-prot->svr_version+1) ;

  /* connection id */
  prot->conn_id = byte4_2_ul(ptr);
  log_debug("connection id 0x%lx\n",prot->conn_id);

  /* the 'auth plugin data' 1st */
  ap_data = ptr + 4 ;
  ptr = ap_data + AP_DATA1_LEN + 1  ;
  memcpy(prot->ap_data,ap_data,AP_DATA1_LEN);

  /* get 1st of capability flag */
  prot->svr_cap = byte2_2_ul(ptr);
  log_debug("server capabilities 1st: 0x%lx\n", prot->svr_cap);
  ptr += 2 ;

  /* test if there're more bytes */
  if (end>=(ptr+16)) {
    /* get character set */
    //prot->charset = ptr[0] ;
    log_debug("server character set: %d\n", /*prot->charset*/ptr[0]);
    ptr ++;

    /* get sever status */
    prot->svr_stat= byte2_2_ul(ptr);
    log_debug("server status: 0x%x\n", prot->svr_stat);
    ptr += 2 ;

    /* get 2nd of capability flag */
    prot->svr_cap|= byte2_2_ul(ptr)<<16;
    log_debug("server capabilities: 0x%lx\n", prot->svr_cap);
    ptr += 2;

    /* get 'auth data len' from server */
    prot->ap_len = ptr[0] - 1 ;
    if (prot->ap_len<0) {
      return -1;
    }

    ptr ++ ;
    log_debug("auth data len: %d\n", prot->ap_len);

    /* skip reserve bytes */
    ptr += 10;
  }

  /* get the reset 'auth plugin data' */
  if (end >= (ptr+AP_DATA2_LEN)) {
    /* rest length dosen't include the tailing '0' of
     *  authentication data */
    size_t rest_ln = prot->ap_len - AP_DATA1_LEN  ;

    //ap_data = ptr ;
    memcpy(prot->ap_data+AP_DATA1_LEN, ptr,rest_ln);
    ptr[rest_ln] = 0;
    ptr += rest_ln + 1 ;
    log_debug("rest auth len: %zu\n", rest_ln);

    /* deal with 'auth plugin name', if exists */
    if (prot->svr_cap & sc_client_auth_plugin) {
      strcpy(prot->ap_name,ptr);
      log_debug("auth plugin name: %s\n", prot->ap_name);
    } else {
      /* indicate an 'auth plugin name' here */
      strcpy(prot->ap_name,DEFAULT_PLUGIN_NAME);
      log_debug("default auth plugin name: %s\n", prot->ap_name);
    }
  }

  return 0;
}

int mysqls_parse_response(mysqlclient_proto_t prot, char *buff, size_t len)
{
  int ret = 0;
  char *end = 0;


  /* the OK status */
  if (!buff[0]) {
    /* skip the error state header */
    buff++ ;
    prot->affected_rows    = lenenc_int_get(&buff);
    //prot->res->rows        = prot->affected_rows ;
    prot->last_inserted_id = lenenc_int_get(&buff);
    return 0;
  }

  /* error status */
  if ((uint8_t)buff[0]==0xff) {
    end = buff + len ;
    /* skip error header */
    buff++ ;

    /* error number */
    ret = byte2_2_ul(buff);
    prot->last_errno = ret ;
    buff+=2 ;

    /* skip the sql state marker '#' */
    buff++ ;

    /* get sql state */
    strncpy(prot->sql_state,buff,5);
    prot->sql_state[5] = '\0';
    buff+= 5;

    /* get error description */
    strncpy(prot->last_error,buff,end-buff);
    prot->last_error[end-buff] = '\0';
    if (ret) {
      log_debug("errno:%d, sqlstate:%s, error:%s\n",
        prot->last_errno, prot->sql_state,
        prot->last_error);
    }
    return -1;
  }

  /* other packet type */
  return 1;
}

char* str_store(char *ptr, const char *str)
{
  size_t ln = 0;


  if (!str || !*str) 
    return ptr ;

  ln = (ln=strlen(str))>MAX_NAME_LEN?(MAX_NAME_LEN-1):ln ;
  ptr = stpncpy(ptr,(char*)str,ln);
  ptr[0] = '\0';

  return ptr+1 ;
}

ssize_t 
mysqlc_gen_login(mysqlclient_proto_t prot, const char *user,
                 const char *passwd,const char *db, 
                 const uint32_t client_flag,char *buff, size_t sz_out)
{
  char *end= buff;
  size_t sz = 0;
  /* max allow packet size */
  const uint32_t sz_max_allow_packet = 1024L*1024L*1024L ;


  /* add user defined capacities */
  prot->client_cap |= client_flag;
  /* set client capability flags */
  prot->client_cap  |= client_long_password | 
    client_long_flag |  client_transactions  |
    client_protocol_41 | client_secure_connection | 
    client_plugin_auth | /*client_connect_attrs | */
    /*client_connect_with_db |*/ client_plugin_auth_lenenc_client_data ;

  if (db) {
    prot->client_cap |= client_connect_with_db ;
  } else {
    prot->client_cap &= (~client_connect_with_db) ;
  }

  /* skip size bytes */
  end += 3;
  /* serial number */
  end[0] = 1 ;
  end++;

  /* write client flags */
  ul4store(prot->client_cap,end);
  log_debug("client flags: 0x%lx\n", prot->client_cap);

  /* max packet size */
  ul4store(sz_max_allow_packet,end+4);
  log_debug("max packet size: %d\n", sz_max_allow_packet);

  /* character set */
  end[8] = prot->charset ;
  end+=9 ;

  /* the reserve bytes */
  bzero(end,23);
  end+=23 ;

  /* user name */
  end = str_store(end,user);

  /* wirte the auth response */
  if (prot->ap_len<=0) {
    *end++ = 0;
  } else {
    /* there're auth response data */
    if (prot->client_cap & client_plugin_auth_lenenc_client_data) {
      end += lenenc_int_set(prot->ap_len,end);
    } else {
      /* ap_len < 255 */
      *end++ = prot->ap_len ;
    }

    /* generate and store 'auth response data' */
    if (gen_auth_data(prot->ap_data,prot->ap_len, passwd,(uint8_t*)end)) {
      log_error("failed generate auth data\n");
      return -1;
    }
    end += prot->ap_len ;

  }

  /* write db */
  if (prot->client_cap & client_connect_with_db) {
    end = str_store(end,db);
  }

  /* the auth plugin name */
  if (prot->client_cap & client_plugin_auth) {
    end = str_store(end,prot->ap_name);
  }

  /* 
   * TODO: write the 'connect attribute' fields 
   */
  /* body size */
  sz = end - buff - 4 ;
  if (sz>sz_out) {
    log_error("packet size %zu too large!\n", sz);
    return -1;
  }

  /* rewrite packet size */
  ul3store(sz,buff);

  /* always starts nr at '1' during 
   *  init hands hake  */
  prot->nr = 1 ;

  return sz+4;
}

