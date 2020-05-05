
#include "socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <time.h>
#include <netinet/tcp.h>
#include "log.h"



int new_udp_svr(int efd, int port)
{
  struct sockaddr_in sa ;
  int fd = 0;

  if ((fd=socket(AF_INET,SOCK_DGRAM,0))<0) {
    /* XXX: error message here */
    return -1;
  }

  set_nonblock(fd);

  sa.sin_family     = AF_INET ;
  sa.sin_addr.s_addr= htonl(INADDR_ANY);
  sa.sin_port       = htons(port);
  if (bind(fd,(struct sockaddr*)&sa,sizeof(sa))<0) {
    /* XXX: error message here */
    log_error("error bind 0x%x:%d(%s)\n", 
      sa.sin_addr.s_addr, port, strerror(errno));
    return -1;
  }
  if (add_to_epoll(efd,fd,0)) {
    /* XXX: error message here */
    log_error("error add dispatcher to epoll(%s)\n",
      strerror(errno));
    return -1;
  }
  return fd;
}

int new_udp_client()
{
  int fd = 0;

  if ((fd=socket(AF_INET,SOCK_DGRAM,0))<0) {
    /* XXX: error message here */
    return -1;
  }
  return fd;
}

int new_tcp_svr(unsigned long address, int port)
{
  uint32_t addr ;
  struct sockaddr_in sa ;
  int flag =1;
  int fd = 0;

  addr = !address?htonl(INADDR_ANY):htonl(address);
  if (addr<0) {
    /* XXX: error message here */
    return -1;
  }
  /* initialize the server socket */
  if ((fd = socket(AF_INET,SOCK_STREAM,0))<0) {
    /* XXX: error message here */
    return -1;
  }

  set_nonblock(fd);

  /* reuse the server address */
  setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&flag,sizeof(int));

  /* bind server address */
  sa.sin_family      = AF_INET ;
  sa.sin_addr.s_addr = addr;
  sa.sin_port        = htons(port);
  if (bind(fd,(struct sockaddr*)&sa,sizeof(struct sockaddr))<0) {
    /* XXX: error message here */
    log_error("error bind 0x%x:%d(%s)\n", 
      addr, port, strerror(errno));
    return -1;
  }
  /* do listening */
  if (listen(fd,/*SOMAXCONN*/10240)<0) {
    /* XXX: error message here */
    log_error("error listen 0x%x:%d\n", addr, port);
    return -1;
  }

  return fd;
}

int new_tcp_client(unsigned long addr, int port)
{
  int fd=0;
  struct sockaddr_in sa;

  if ((fd=socket(AF_INET,SOCK_STREAM,0))<0) {
    /* XXX: error message here */
    return -1;
  }
  sa.sin_family      = AF_INET;
  sa.sin_addr.s_addr = htonl(addr);
  sa.sin_port        = htons(port);
  if (connect(fd,(struct sockaddr*)&sa,
     sizeof(struct sockaddr))<0) {
    log_error("cant connect to host: %s\n",
      strerror(errno));
    close(fd);
    return -1;
  }
  set_nonblock(fd);

  return fd;
}

int new_tcp_socket()
{
  return socket(AF_INET,SOCK_STREAM,0);
}

int new_tcp_client2(int clientfd, unsigned long addr, int port)
{
  struct sockaddr_in sa = {
    .sin_family      = AF_INET,
    .sin_addr.s_addr = htonl(addr),
    .sin_port        = htons(port),
  };


  if (connect(clientfd,(struct sockaddr*)&sa,
     sizeof(struct sockaddr))<0) {
    log_error("cant connect to host: %s\n",
      strerror(errno));
    close(clientfd);
    return -1;
  }
  set_nonblock(clientfd);

  return 0;
}

int do_accept(int sfd, void *client_info)
{
  struct sockaddr in_addr;  
  socklen_t in_len = sizeof(in_addr);  
  int fd = 0, flag = 1;
  
  /* no more inbound connections */
  if ((fd = accept(sfd,&in_addr,&in_len))<0 /*&& errno==EAGAIN*/) {
    return -1 ;
  }

  set_nonblock(fd);

  /* set tcp no delay */
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
  flag = 0;
  setsockopt(fd, IPPROTO_TCP, TCP_CORK, &flag, sizeof(int));

  if (client_info) 
    memcpy(client_info,&in_addr,sizeof(struct sockaddr_in));

  return fd;
}

int new_socketpair(int efd, int fds[])
{
  if (socketpair(PF_UNIX,SOCK_STREAM,0,fds)==-1) {
    return -1;
  }

  set_nonblock(fds[1]);

  return 0;
}

int brocast_tx(int fd, int bcast_port, char *data, size_t len)
{
  int i = 0;
  struct ifreq *ifr;
  struct ifconf ifc;
  char buf[1024];
  struct sockaddr_in sa ;

  /* iterate all local interfaces */
  ifc.ifc_len = sizeof(buf);
  ifc.ifc_buf = buf;
  if (ioctl(fd,SIOCGIFCONF,(char*)&ifc)<0) {
    /* XXX: error message here */
    return -1;
  }
  /* send advertisement to each interface */
  sa.sin_family = AF_INET;
  sa.sin_port   = htons(bcast_port);
  for (i=ifc.ifc_len/sizeof(struct ifreq),
    ifr = ifc.ifc_req; --i>=0; ifr++) 
  {
    if (ioctl(fd,SIOCGIFBRDADDR,ifr)<0)
      continue ;
    /* set broadcast address */
    sa.sin_addr.s_addr = ((struct sockaddr_in*)&ifr->
      ifr_broadaddr)->sin_addr.s_addr ;
    /* enable broadcast */
    setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&sa,
      sizeof(sa));
    /* send the packet */
    sendto(fd,(char*)data,len,MSG_DONTWAIT,
      (struct sockaddr*)&sa,sizeof(sa));
  }

  return 0;
}

unsigned long hostname_to_uladdr(const char *host)
{
  struct hostent *ent = gethostbyname(host);

  if (ent) {
    struct in_addr *s_in= (struct in_addr*)ent->h_addr ;
    return htonl(s_in->s_addr);
  }

  return 0L;
}

int sock_close(int fd)
{
  //return close(fd);
  
  return shutdown(fd,SHUT_RDWR);
}
