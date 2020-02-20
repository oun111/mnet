#ifndef __RUNTIME_RC_H__
#define __RUNTIME_RC_H__


struct runtime_rc_s {
  void *handle ;

  int (*save) (struct runtime_rc_s*,int,char*,void*);
  int (*fetch)(struct runtime_rc_s*,int,char*,void*);
} ;
typedef struct runtime_rc_s* runtime_rc_t ;


enum rrc_types {
  rt_na,
  rt_orders,
  rt_amount,
  rt_max,
};


extern void init_runtime_rc(runtime_rc_t entry, void *handle);

extern int runtime_rc_save(runtime_rc_t entry, int type, char *appid, void *value);

extern int runtime_rc_fetch(runtime_rc_t entry, int type, char *appid, void *value);

#endif /* __RUNTIME_RC_H__*/
