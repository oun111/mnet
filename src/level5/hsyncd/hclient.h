#ifndef __HCLIENT_H__
#define __HCLIENT_H__


#include <thrift/c_glib/thrift.h>
#include <thrift/c_glib/thrift_application_exception.h>
#include <thrift/c_glib/protocol/thrift_binary_protocol.h>
#include <thrift/c_glib/protocol/thrift_compact_protocol.h>
#include <thrift/c_glib/protocol/thrift_multiplexed_protocol.h>
#include <thrift/c_glib/transport/thrift_buffered_transport.h>
#include <thrift/c_glib/transport/thrift_framed_transport.h>
#include <thrift/c_glib/transport/thrift_ssl_socket.h>
#include <thrift/c_glib/transport/thrift_socket.h>
#include <thrift/c_glib/transport/thrift_transport.h>
#include "t_h_base_service.h"


struct hbase_client_s {
  ThriftSocket *socket ;

  ThriftProtocol *protocol ;

  THBaseServiceIf *client ;

  ThriftTransport *transport ;
} ;
typedef struct hbase_client_s* hclient_t ;



extern int hclient_init(hclient_t cln, const char *host, const int cport);

extern void hclient_release(hclient_t cln);

extern int hclient_exists(hclient_t cln, const char *tablename);

extern int hclient_get(hclient_t cln, const char *tablename, const char *row, 
                       const char *cf, char *val);

extern int hclient_put(hclient_t cln, const char *tablename, 
                       const char *row, const char *cf, const char *val);

extern int hclient_delete(hclient_t cln, const char *tablename, 
                          const char *row, const char *cf);

#endif /* __HCLIENT_H__*/

