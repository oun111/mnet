#ifndef __QRCODE_CACHE_H__
#define __QRCODE_CACHE_H__


struct qrcode_cache_s {
  void *handle ;

  char name[32];
} ;
typedef struct qrcode_cache_s* qrcode_cache_t ;


extern void init_qrcode_cache(qrcode_cache_t entry, void *handle, char *name);

extern int qrcode_cache_save(qrcode_cache_t entry, char *orderid, const char *qrcode);

extern int qrcode_cache_fetch(qrcode_cache_t entry, char *orderid, dbuffer_t *qrbuf);

#endif /* __QRCODE_CACHE_H__*/
