#ifndef __SOCKET_H__
#define __SOCKET_H__

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "connection.h"


#define __net_atoi(s) ({\
  int i0,i1,i2,i3;\
  sscanf(s,"%d.%d.%d.%d", &i0,&i1,&i2,&i3);\
  (i0<<24)|(i1<<16)|(i2<<8)|i3 ;\
})

#define init_epoll()  ({\
  epoll_create(1) ; \
})
#define close_epoll(ef) ({\
  close(ef);\
})

/* add handle to epoll */
#define add_to_epoll(ef,tf,po) ({     \
  struct epoll_event event;           \
  if (po) event.data.ptr = (void*)((uintptr_t)po) ; \
  else event.data.fd = tf ;      \
  event.events  = EPOLLIN|EPOLLET  ;  \
  epoll_ctl((ef),EPOLL_CTL_ADD,(tf),&event);\
})
  /* remove handle from epoll */
#define del_from_epoll(ef,tf) ({     \
  /*struct epoll_event event;*/          \
  epoll_ctl((ef),EPOLL_CTL_DEL,(tf),NULL) ;\
})
  /* modify epoll event */
#define mod_epoll(ef,tf,ev,po) ({       \
  struct epoll_event event;          \
  if (po)  event.data.ptr = (void*)((uintptr_t)po); \
  else event.data.fd = tf ;            \
  event.events  = /*EPOLLIN|EPOLLET|*/(ev) ;     \
  epoll_ctl((ef),EPOLL_CTL_MOD,(tf),&event) ;\
})

  /* add 'EPOLLOUT' event */
#define enable_send(ef,tf,po)  mod_epoll(ef,tf,EPOLLOUT|EPOLLET,po)
  /* del 'EPOLLOUT' event */
#define disable_send(ef,tf,po)  mod_epoll(ef,tf,EPOLLIN|EPOLLET,po)

  /* make handle non-blocking */
#define set_nonblock(fd)  \
  fcntl((fd),F_SETFL, \
    fcntl((fd),F_GETFL,0)| O_NONBLOCK);
#define set_block(fd) \
  fcntl((fd),F_SETFL,fcntl((fd),F_GETFL,0)&~O_NONBLOCK)


extern int new_udp_svr(int,int);

extern int new_udp_client();

extern int new_tcp_svr(unsigned long,int);

extern int new_tcp_client(unsigned long,int);

extern int do_accept(int sfd, void *client_info);

extern int new_tcp_socket();

extern int new_tcp_client2(int clientfd, unsigned long addr, int port);

extern unsigned long hostname_to_uladdr(const char *host);

extern int sock_close(int fd);

extern int new_socketpair(int efd, int fds[]);

#endif /* __SOCKET_H__ */

