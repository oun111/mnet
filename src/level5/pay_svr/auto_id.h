#ifndef __AUTO_ID_H__
#define __AUTO_ID_H__



struct auto_id_s {
  void *myrds_handle ;
  char cache_name[RDS_NAME_SZ];
  char mq_name[RDS_NAME_SZ];
  char var_name[RDS_NAME_SZ];
  char id_name[32]; 
  /*unsigned*/long long val;
  char alias_val[96];
} ;
typedef struct auto_id_s *auto_id_t ;


extern int aid_init(auto_id_t id, char *name, void *hdl);

extern void aid_reset(auto_id_t id);

extern char* aid_add_and_fetch(auto_id_t id,unsigned long v);

extern char* aid_fetch(auto_id_t id);

#endif /* __AUTO_ID_H__*/
