/*
 *  Module Name:   mysqls
 *
 *  Description:   implements a tiny mysql server libary
 *
 *  Author:        yzhou
 *
 *  Last modified: Sep 7, 2018
 *  Created:       Sep 21, 2016
 *
 *  History:       refer to 'HISTROY.TXT'
 *
 */ 
#ifndef __MYSQLS_H__
#define __MYSQLS_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdbool.h>

#ifdef __VER_STRING__
#undef __VER_STRING__
#endif
/* module version string */
#define __VER_STRING__  "v0.00.2"


#ifdef __cplusplus
extern "C" {
#endif


/* total auth plugin data len */
#define AP_LENGTH  21

/* max length of a password */
#define MAX_PWD_LEN  512

typedef struct mysqls_global_var_t
{
  uint8_t char_set ;
  uint16_t server_status ;
} MYSQLS_VAR;


struct mysqlclient_proto_s {
  int reconnect;
  /* packet number */
  unsigned long nr ;
  /* version */
  unsigned char proto_version;
  char svr_version[256];
  /* connection id */
  unsigned long conn_id ;
  /* capability flag */
  unsigned long svr_cap ;
  /* character set */
  unsigned char charset ;
  /* server status */
  unsigned int svr_stat;
  /* auth data plugin management */
  char ap_data[256];
  unsigned char ap_len ;
  char ap_name[64];
  /* client capability flags */
  unsigned long client_cap ;
  /* error management */
  int num_warning;
  unsigned long long affected_rows ;

  /* yzhou added */
  unsigned long long last_inserted_id ;

#if 0
  /* the command buffer */
  dbuffer_t cmd_buff ;
  /* columns info set */
  dbuffer_t columns;
  /* raw result set */
  MYSQL_RES *res ;
#endif

  /* error management */
#define MAX_ERRMSG  512
#define MAX_SQLSTATE  6
  char last_error[MAX_ERRMSG+1];
  unsigned int last_errno ;
  char sql_state[MAX_SQLSTATE+1];

#if 0
  /* 
   * the uncompressed data buffer 
   */
  container ucomp_buff,
  /* the uncompressed tmp data buffer */
  ucomp_tmp;

  /* compress mode */
  int compress ;
#endif

  /* time stamp when connected OK */
#if 1
  unsigned long long ts_ok ;
#else
  struct timestamp_t ts_ok ;
#endif

  /* fast rx mode */
  //int fast_rx ;
} ;

typedef struct mysqlclient_proto_s* mysqlclient_proto_t ;


/* check packet by size */
extern bool mysqls_is_packet_valid(char*,size_t);

/* check if it's eof packet */
extern bool mysqls_is_eof(char*,size_t);

/* generate initial greeting packet */
extern int mysqls_gen_greeting(uint32_t,
  int,uint32_t,char*,char*,char*,size_t);

/* generates random string */
extern void mysqls_gen_rand_string(char*,size_t sz);

/* test if input packet is login request */
extern bool mysqls_is_login_req(char*,size_t);

/* parse login request */
extern int mysqls_parse_login_req(char*,size_t, 
  char*,char*,size_t*,char*) ;

/* authenticates user passwords */
extern int mysqls_auth_usr(char*,char*,
  char*,size_t,char*,size_t);

/* generates an Ok response packet */
extern int mysqls_gen_ok(char*,uint16_t,uint32_t,uint64_t,uint64_t);

/* generates an Error response packet */
extern int mysqls_gen_error(char*,uint16_t, 
  int,uint32_t,va_list);
extern int do_err_response(uint32_t, char*,int,int , ...);
extern int do_ok_response(uint32_t,int,char[/*MAX_PAYLOAD*/]);
extern uint8_t mysqls_is_error(char*,size_t);

/* generates response for query field command */
extern int mysqls_gen_qry_field_resp(char *inb, uint16_t sql_stat, 
  int char_set,uint32_t sn, char *db, char *tbl, char **rows, size_t numRows);

/* get serial number of a packet */
extern int mysqls_extract_sn(char*);

/* get body position */
extern char* mysqls_get_body(char*);

extern int mysqls_sprintf(char*,int,va_list) ;

extern int mysqls_extract_column_count(char*,size_t) ;

extern char* mysqls_save_one_col_def(void *pc, char *ptr, char *def_db);

extern int mysqls_extract_prepared_info(char *inb, 
  size_t sz, int *stmtid, int *nCol, int *nPh) ;

extern int mysqls_get_col_full_name(void*,int,char**,
  char**,char**,uint8_t*);

extern bool mysqls_is_digit_type(uint8_t);

extern void mysqls_update_sn(char*,uint8_t);

extern uint8_t mysqls_get_cmd(char *inb);

extern void mysqls_set_cmd(char *inb, uint8_t cmd);

extern int mysqls_get_stmt_prep_stmt_id(char*,size_t,int*);

extern int mysqls_update_stmt_prep_stmt_id(char*,size_t,int);

extern size_t mysqls_get_body_size(char*);

extern size_t mysqls_get_req_size(char*);

extern void mysqls_update_req_size(char*,size_t);

extern ssize_t mysqls_gen_normal_resp(char *inb, uint16_t sql_stat, 
  int char_set, uint32_t sn, char *db, char *tbl, char **cols, 
  size_t num_cols, char **rows, size_t num_rows);

extern int mysqls_gen_stmt_close(char *inb, int stmtid);

extern size_t mysqls_gen_rollback(char *inb);

extern int mysqlc_parse_greeting(mysqlclient_proto_t prot, char *inb, 
    size_t sz_in);

extern ssize_t mysqlc_gen_login(mysqlclient_proto_t prot,
  const char *user,const char *passwd,
  const char *db, const uint32_t client_flag,
  char *buff, size_t sz_out) ;

extern int mysqls_parse_response(mysqlclient_proto_t prot, char *buff, size_t len);

#ifdef __cplusplus
}
#endif

#endif /*  __MYSQLS_H__ */

