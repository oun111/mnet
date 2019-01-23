#ifndef __INSTANCE_H__
#define __INSTANCE_H__

#include "proto.h"
#include "connection.h"


enum proto_types {
  local_l4,
  normal_l4,
  inbound_l5,
  outbound_l5,
  max_protos,
} ;

//#include "module.h"


extern int instance_start(int argc, char *argv[]);

extern int instance_stop();

extern Network_t get_current_net();

extern int ext_get_max_connections();


#endif /* __INSTANCE_H__*/
