#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include "kernel.h"
#include "mm_porting.h"
#include "rbtree.h"
#include "myrbtree.h"
#include "hsyncd_rpc.h"
#include "log.h"
#include <rpc/pmap_clnt.h>


static
struct rpcSvcInfo_s {
  rpc_rx_cb local_cb ;

  struct rb_root root;
}
g_rpcSvcInfo = {
  .local_cb = NULL,

  .root = RB_ROOT,
} ;

struct xprtItem_s {
  int fd ;

  SVCXPRT *xprt ;

  struct rb_node node ;
} ;
typedef struct xprtItem_s* xprtItem_t ;


static int compare(int fd0, int fd1)
{
  return fd0>fd1?1:fd0<fd1?-1:0 ;
}

int rpc_svc_rx(int fd)
{
  xprtItem_t xp = NULL ;



  // I can not access the 'static SVCXPRT **xports' defined 
  // in   'sunrpc/svc.c',  so   save  the  xprt  item  here
  if (MY_RB_TREE_FIND(&g_rpcSvcInfo.root,fd,xp,fd,node,compare)) {
    xp = kmalloc(sizeof(struct xprtItem_s),0L);
    xp->xprt = svcfd_create(fd,0,0);
    xp->fd = fd ;

    if (MY_RB_TREE_INSERT(&g_rpcSvcInfo.root,xp,fd,node,compare)) {
      log_error("insert xprt item for fd %d fail\n",fd);
      kfree(xp);
      return -1;
    }
  }

  svc_getreq_common(xp->fd);

  // xprt.xp_sock==0 also represents  the xprt.xp_p1  is  null 
  // or  other  severe  errors that I  don't  understand, this
  // happens  when  the 35s-RPC-idle-time (refer to glibc-2.27
  // source/sunrpc/svc_tcp.c:317) reaches and will lead to   a
  // crash in the   subsequent   SVC_STAT() call, so  do   NOT  
  // process  this connection   any   more.
  if (xp->xprt->xp_sock!=fd || SVC_STAT(xp->xprt)==XPRT_DIED) {

    log_debug("xprt/fd-%d close\n",xp->fd);
    rb_erase(&xp->node,&g_rpcSvcInfo.root);
    kfree(xp);
    return -1;
  }

  return 0;
}

bool_t my_decode_arguments(XDR *xdrs, char **sp)
{
  *sp = malloc(sizeof(struct rpc_data_s));
  xdrs->x_ops->x_getbytes(xdrs,*sp,sizeof(struct rpc_data_s));

  return 1;
}

static void
rpc_svc(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		char *rpc_sendh_1_arg;
	} argument;
	char *result;
	xdrproc_t _xdr_argument, _xdr_result;


	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply (transp, (xdrproc_t) xdr_void, (char *)NULL);
		return;

	case rpc_sendh:
		_xdr_argument = (xdrproc_t) xdr_wrapstring;
		_xdr_result = (xdrproc_t) xdr_wrapstring;
		//g_rpcSvcInfo.local_cb = (char *(*)(char *, struct svc_req *)) rpc_sendh_1_svc;
		break;

	default:
		svcerr_noproc (transp);
		return;
	}
	memset ((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs (transp, (xdrproc_t) /*_xdr_argument*/my_decode_arguments, (caddr_t) &argument)) {
		svcerr_decode (transp);
		return;
	}

	result = (char*)(*g_rpcSvcInfo.local_cb)((char **)&argument, rqstp);

	if (result != NULL && !svc_sendreply(transp, (xdrproc_t) _xdr_result, result)) {
		svcerr_systemerr (transp);
	}
	if (!svc_freeargs (transp, (xdrproc_t) _xdr_argument, (caddr_t) &argument)) {
		fprintf (stderr, "%s", "unable to free arguments");
		exit (1);
	}
	return;
}

int rpc_svc_init(int vernum, int *retfd, rpc_rx_cb cb)
{
  int fd = socket(AF_INET,SOCK_STREAM|SOCK_NONBLOCK,0);
	register SVCXPRT *transp = svctcp_create(fd, 0, 0);


	if (transp == NULL) {
		log_error("cannot create tcp service.");
    return -1;
	}

	pmap_unset (hsyncdRpc, hsyncdRpcVer+vernum);

	if (!svc_register(transp, hsyncdRpc, hsyncdRpcVer+vernum, rpc_svc, IPPROTO_TCP)) {
		log_error("unable to register (hsyncdRpc, hsyncdRpcVer, tcp).");
    return -1;
	}

  if (retfd) {
    *retfd = fd ;
    log_debug("new svc fd: %d\n",fd);
  }

  if (cb) {
    g_rpcSvcInfo.local_cb = cb;
  }

  log_debug("pid %d: rpc %d %d registered\n",getpid(),hsyncdRpc,hsyncdRpcVer+vernum);

  return 0;
}

int rpc_clnt_init(rpc_clnt_t clnt, int v)
{
  CLIENT *c = clnt_create("127.0.0.1", hsyncdRpc, hsyncdRpcVer+v, "tcp");


  clnt->c = NULL;
  clnt->v = v ;

  if (!c) {
    log_error("create rpc client(prog: %d, ver: %d) fail\n",
              hsyncdRpc, hsyncdRpcVer+v);
    return -1 ;
  }

  clnt->c = c ;

  return 0 ;
}

void rpc_clnt_release(rpc_clnt_t clnt)
{
  if (clnt->c) {
    clnt_destroy(clnt->c);
  }
}

bool_t my_encode_arguments(XDR *xdrs, char **sp)
{
  xdrs->x_ops->x_putbytes(xdrs,(char*)*sp,sizeof(struct rpc_data_s));
  return 1;
}

int rpc_clnt_tx(rpc_clnt_t clnt, void *data)
{
  char *clnt_res = 0;
  char *argp = data;
  struct timeval TIMEOUT = { 25, 0 };
  int ret = -1;


  // glibc-2.27/sunrpc/svc_tcp.c:317: idle connections would 
  //  be  closed  after  35s, so  needs  reconnection   here
  while(1) {
    ret = -1;
    clnt_res = 0;

    if (clnt->c) {
      ret = clnt_call (clnt->c, rpc_sendh,
        (xdrproc_t) my_encode_arguments, (caddr_t) &argp,
        (xdrproc_t) xdr_wrapstring, (caddr_t) &clnt_res,
        TIMEOUT) ;
    }

    if (ret==RPC_SUCCESS)
      break ;

    if (clnt_res)
      free(clnt_res);

    // try reconnect
    rpc_clnt_release(clnt);

    if (rpc_clnt_init(clnt,clnt->v)) {
      log_error("re-connect rpc server(prog:%d,ver:%d) fail!\n",
                hsyncdRpc,(hsyncdRpcVer+clnt->v));
      return -1;
    }
  }

  log_debug("server res: %s\n",clnt_res);
  free(clnt_res);

  return 0;
}

