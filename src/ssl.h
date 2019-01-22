#ifndef __SSL_H__
#define __SSL_H__

#include <openssl/ssl.h>

struct ssl_item_s {
  SSL_CTX *ctx ;
  SSL *ssl ;
} ;
typedef struct ssl_item_s* ssl_item_t ;


extern ssl_item_t ssl_client_init();

extern void ssl_release(ssl_item_t ps);

#endif /* __SSL_H__*/
