/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _HSYNCD_RPC_H_RPCGEN
#define _HSYNCD_RPC_H_RPCGEN

#include <rpc/rpc.h>
#include <rpc/clnt.h>

#define hsyncdRpc 99
#define hsyncdRpcVer 1
#define rpc_sendh 1


typedef char**(*rpc_rx_cb)(char**, struct svc_req *);


// my own argument struct
#if 0
struct rpc_data_s {
  char str[64];
  int val;
  char str2[128];
  long num ;
} ;
#else
struct rpc_data_s {
  char rowno[64];
  char table[128];
  char k[256];
  char v[2048];
} __attribute__((__aligned__(64))) ;
#endif

struct rpc_clnt_s {
  CLIENT *c ;
  int v ;
} ;
typedef struct rpc_clnt_s* rpc_clnt_t ;


extern int rpc_svc_init(int,int*,rpc_rx_cb);

extern int rpc_svc_rx(int fd);

extern int rpc_clnt_init(rpc_clnt_t, int);

extern void rpc_clnt_release(rpc_clnt_t);

extern int rpc_clnt_tx(rpc_clnt_t clnt, void *data);

#endif /* !_HSYNCD_RPC_H_RPCGEN */

