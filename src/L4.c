#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include "proto.h"
#include "socket.h"
#include "mm_porting.h"
#include "L4.h"
#include "instance.h"
#include "log.h"


/**
 * default L4 protocol opts
 */

int tcp_rx(Network_t net, connection_t pconn)
{
  const size_t rx_size = 4096 ;
  ssize_t ret = 0;
  dbuffer_t buf ;

  pconn->rxb = rearrange_dbuffer(pconn->rxb,rx_size);
  /* arrange buffer fail */
  if (!pconn->rxb) {
    log_error("alloca rx buffer fail\n");
    return -1;
  }

  /* the read data position */
  buf = dbuffer_ptr(pconn->rxb,1);

  ret = read(pconn->fd,buf,rx_size);
  if (ret<=0) {
    /* peer closed */
    if (!ret) 
      return -1;

    /* no more data to read */
    if (errno==EAGAIN || errno==EWOULDBLOCK)
      return 1;

    return -1;
  }

  /* update write pointer */
  dbuffer_lseek(pconn->rxb,ret,SEEK_CUR,1);

  return 0;
}

int tcp_tx(Network_t net, connection_t pconn)
{
  size_t ret = 0;
  size_t tx_size = dbuffer_data_size(pconn->txb);
  dbuffer_t buf = dbuffer_ptr(pconn->txb,0);

  if (tx_size==0L)
    return 1;

  ret = write(pconn->fd,buf,tx_size);
  if (ret<0) {
    /* tx buffer is full, cant send */
    if (errno==EAGAIN || errno==EWOULDBLOCK) 
      goto __resend;

    return -1;
  }

  /* update read pointer */
  dbuffer_lseek(pconn->txb,ret,SEEK_CUR,0);

  if (ret==tx_size) {
    disable_send(net->m_efd,pconn->fd,pconn);
    return 0;
  }

  /* not all the data are send */
__resend:
  enable_send(net->m_efd,pconn->fd,pconn);
  return 1;
}

int tcp_accept(Network_t net, connection_t pconn)
{
  int fd = 0, sfd = pconn->fd;
  int ret = 0;
  struct sockaddr_in sa ;

  while (1) {

    /* inbound client fd */
    fd = do_accept(sfd,&sa);
    if (fd<0) 
      break ;

    {
      connection_t inbound_conn = net->reg_inbound(net,fd,pconn->module_id);

      if (!inbound_conn) {
        log_error("disconnect client %s:%d\n",
                  inet_ntoa(sa.sin_addr),htons(sa.sin_port));
        close(fd);
        continue ;
      }

      /* triger an 'L5' send event if needed */
      ret = inbound_conn->l5opt.greeting(net,inbound_conn);

      if (!ret) 
        enable_send(net->m_efd,fd,inbound_conn);

      else if(ret<0) {
        log_error("greeting fail, shutdown client %s:%d\n",
                  inet_ntoa(sa.sin_addr),htons(sa.sin_port));
        tcp_close(net,inbound_conn);
      }

    }
  }

  return 1;
}

void tcp_close(Network_t net, connection_t pconn)
{
  net->unreg_all(net,pconn);

  close(pconn->fd);
}

