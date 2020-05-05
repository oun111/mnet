
#ifndef __PROTO_H__
#define __PROTO_H__

typedef struct connection_s* connection_t;
typedef struct Network_s* Network_t ;

enum proto_types {
  local_l4,
  normal_l4,
  inbound_l5,
  outbound_l5,
  max_protos,
} ;

/**
 * protocol operations
 */
typedef struct protocol_operation_s {
  int (*init)(Network_t);
  void(*release)();
  int (*rx)(Network_t,connection_t);
  int (*tx)(Network_t,connection_t);
  int (*greeting)(Network_t,connection_t);
  void(*close)(Network_t,connection_t);
} proto_opt ;

#define NULL_PROTO_OPT  {NULL,NULL,NULL,NULL,NULL,NULL}


extern int register_proto_opt(connection_t pconn, 
    int proto_type, proto_opt *opt);

extern int unregister_proto_opt(connection_t pconn, int proto_type);

extern void reset_proto_opt(proto_opt *opt);

#endif /* __PROTO_H__*/
