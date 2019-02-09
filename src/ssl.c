#include <openssl/err.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include "ssl.h"
#include "mm_porting.h"
#include "log.h"
#include "socket.h"


#define SSL_LOG_ERROR() do{ \
  int __rc__ = ERR_get_error(); \
  char __msg__[512] = ""; \
  ERR_error_string(__rc__, __msg__); \
  log_error("fail, %d, %s\n",__rc__,__msg__); \
} while(0)

ssl_item_t ssl_init()
{
  ssl_item_t ps = kmalloc(sizeof(struct ssl_item_s),0L);


  SSL_load_error_strings();
  OpenSSL_add_all_algorithms();
  SSL_library_init();

  ps->ctx = SSL_CTX_new(SSLv23_client_method());
  if (!ps->ctx) {
    SSL_LOG_ERROR();
    return NULL ;
  }

  ps->ssl = SSL_new(ps->ctx);
  if (!ps->ssl) {
    SSL_LOG_ERROR();
    return NULL ;
  }

  ps->state = s_none;
  //ps->peer  = NULL ;

  return ps;
}

void ssl_release(ssl_item_t ps)
{
  if (ps->ssl) {
    SSL_shutdown(ps->ssl);
    SSL_free(ps->ssl);
  }

  if (ps->ctx) {
    SSL_CTX_free(ps->ctx);
  }

  kfree(ps);
}

int ssl_connect(ssl_item_t ps, int fd)
{
  int ret = 0;


  ps->state = s_connecting;

  if (fd>0 && !SSL_set_fd(ps->ssl,fd)) {
    SSL_LOG_ERROR();
    return -1;
  }

  ret = SSL_connect(ps->ssl);
  if (ret==1) {
    log_info("handshake OK!\n");

    ps->state = s_ok ;
    return 1;
  }

  // handshake not completed
  if (SSL_get_error(ps->ssl,ret)==SSL_ERROR_WANT_READ) {
    //log_info("handshake incompleted!\n");
    return 0;
  }

  ps->state = s_error ;

  //SSL_LOG_ERROR();
  return -1;
}

int ssl_read(ssl_item_t ps, char *inb, int inlen)
{
  int ret = SSL_read(ps->ssl,inb,inlen);


  if (ret>0) {
    return ret ;
  }

  // recv incompleted
  if (SSL_get_error(ps->ssl,ret)==SSL_ERROR_WANT_READ) {
    return 0;
  }

  SSL_LOG_ERROR();
  return -1;
}

int ssl_write(ssl_item_t ps, char *outb, int outlen)
{
  int ret = SSL_write(ps->ssl,outb,outlen);


  if (ret>0) {
    return ret ;
  }

  // write incompleted
  if (SSL_get_error(ps->ssl,ret)==SSL_ERROR_WANT_WRITE) {
    return 0;
  }

  SSL_LOG_ERROR();
  return -1;
}

int ssl_rx(Network_t net, connection_t pconn)
{
  ssl_item_t ps = pconn->ssl ;
  const size_t rx_size = 4096 ;
  ssize_t ret = 0;
  dbuffer_t buf ;


  if (!ps) {
    log_error("fatal: no ssl item defined!\n");
    return -1;
  }

  // need ssl-connect
  if (ps->state!=s_ok) {
    ret = ssl_connect(ps,pconn->fd);

    // ssl connection's just ok!
    if (ret==1)
      enable_send(net->m_efd,pconn->fd,pconn);

    return 0;
  }

  pconn->rxb = rearrange_dbuffer(pconn->rxb,rx_size);
  /* arrange buffer fail */
  if (!pconn->rxb) {
    log_error("alloca rx buffer fail\n");
    return -1;
  }

  /* the read data position */
  buf = dbuffer_ptr(pconn->rxb,1);

  ret = ssl_read(ps,buf,rx_size);

  if (ret>0) {
    /* update write pointer */
    dbuffer_lseek(pconn->rxb,ret,SEEK_CUR,1);
    return 0;
  }

  return ret==0?1:-1;
}

int ssl_tx(Network_t net, connection_t pconn)
{
  ssl_item_t ps = pconn->ssl ;
  size_t tx_size = dbuffer_data_size(pconn->txb);
  dbuffer_t buf = dbuffer_ptr(pconn->txb,0);
  int ret = 0;


  if (!ps) {
    log_error("fatal: no ssl item defined!\n");
    return -1;
  }

  if (tx_size==0) {
    return 1;
  }

  ret = ssl_write(ps,buf,tx_size);

  if (ret>=0) {
    /* update read pointer */
    if (ret>0) 
      dbuffer_lseek(pconn->txb,ret,SEEK_CUR,0);

    if (ret==tx_size) {
      disable_send(net->m_efd,pconn->fd,pconn);
      return 0;
    }

    enable_send(net->m_efd,pconn->fd,pconn);
    return 1;
  }

  return -1;
}

void ssl_close(Network_t net, connection_t pconn)
{
  ssl_item_t ps = pconn->ssl ;


  if (!ps) {
    log_error("fatal: no ssl item defined!\n");
    return ;
  }

  ssl_release(ps);
  pconn->ssl = NULL;

  net->unreg_all(net,pconn);

  close(pconn->fd);
}

#if TEST_CASES==1
void test_ssl()
{
#define REQ_BUF_SZ   128
  const char *host = "www.163.com";
  const int port = 443 ;
  int fd = 0, ret = 0;
  unsigned long addr = hostname_to_uladdr(host);
  char dataBuf[REQ_BUF_SZ] = "GET / \r\n\r\n";
  size_t total = 0L ;


  fd = new_tcp_client(addr,port);
  if (fd<=0) {
    printf("fail connect to %s:%d\n",host,port);
    return ;
  }

  ssl_item_t ps = ssl_init();
  if (!ps) {
    return ;
  }

  // FIXME: needs asyn call
  for (;(ret=ssl_connect(ps,fd))==0;fd=-1) {
  }

  if (ret<0) {
    goto __end ;
  }

  // send request
  size_t sz = strlen(dataBuf);

  // FIXME: needs asyn call
  for (;sz>0;sz-=ret) {
    ret = ssl_write(ps,dataBuf,sz);
    if (ret==-1)
      break ;
  }

  printf("receiving response...\n");

  // FIXME: needs asyn call
  for (;(ret=ssl_read(ps,dataBuf,80))>=0;) {
    if (ret>0) {
      dataBuf[ret] = '\0'; 
      printf("%s",dataBuf);
      total += ret ;
    }
  }

  printf("total size: %zu\n",total);

__end:
  ssl_release(ps);

}
#endif

