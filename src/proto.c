#include <string.h>
#include "proto.h"
#include "connection.h"
#include "log.h"



int default_proto_rx(Network_t net, connection_t pconn)
{
  log_debug("invoked\n");
  return 0;
}

int default_proto_tx(Network_t net, connection_t pconn)
{
  log_debug("invoked, fd %d\n",pconn->fd);
  return 0;
}

int default_proto_greeting(Network_t net, connection_t pconn)
{
  log_debug("invoked\n");
  return 1;
}

void default_proto_close(Network_t net,connection_t pconn)
{
  log_debug("invoked\n");
}

int default_proto_preinit(int argc, char *argv[])
{
  log_debug("invoked\n");
  return 0;
}

void default_proto_release()
{
  log_debug("invoked\n");
}

int default_proto_init(Network_t net)
{
  log_debug("invoked\n");
  return 0;
}

int register_proto_opt(connection_t pconn, 
    int proto_type, proto_opt *opt)
{
  proto_opt *popt = proto_type==0 ? 
    &pconn->l4opt:
    &pconn->l5opt;

  if (opt) 
    memcpy(popt,opt,sizeof(proto_opt));

  if (!popt->rx) 
    popt->rx = default_proto_rx ;

  if (!popt->tx) 
    popt->tx = default_proto_tx ;

  if (!popt->greeting) 
    popt->greeting = default_proto_greeting ;

  if (!popt->close) 
    popt->close = default_proto_close ;

  if (!popt->init) 
    popt->init = default_proto_init ;

  if (!popt->release) 
    popt->release = default_proto_release ;

  return 0;
}

int unregister_proto_opt(connection_t pconn, int proto_type)
{
  proto_opt *popt = proto_type==0 ? 
    &pconn->l4opt:
    &pconn->l5opt;

  reset_proto_opt(popt);
  return 0;
}

void reset_proto_opt(proto_opt *opt)
{
  opt->rx     = NULL,
  opt->tx     = NULL,
  opt->greeting = NULL ;
  opt->close  = NULL ;
  opt->init   = NULL ;
  opt->release   = NULL ;
}

