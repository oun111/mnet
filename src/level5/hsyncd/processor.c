#include <unistd.h>
#include "instance.h"
#include "module.h"
#include "rpc.h"
#include "hclient.h"
#include "log.h"


static
struct processorInfo_s {

  struct {
    struct hbase_client_s clt ;

    char host[32];

    int port ;
  } hbase ;

  int local_vernum ;

  int svc_fd ;

} g_procInfo = {
  .local_vernum = -1,

  .svc_fd = -1,
};

static int proc_init(Network_t net);


static
int proc_tx(Network_t net, connection_t pconn)
{
  return 0;
}

static
int proc_rx(Network_t net, connection_t pconn)
{
  return rpc_svc_rx(pconn->fd);
}

static
int proc_dummy_l4_rx(Network_t net, connection_t pconn)
{
  return 0;
}

static
int proc_dummy_l4_tx(Network_t net, connection_t pconn)
{
  return 0;
}

static
void proc_release()
{
}

static char **
proc_rpc_rx(char **argp, struct svc_req *rqstp)
{
	static char * result= "ok!";

	/*
	 * insert server code here
	 */
  struct rpc_data_s *targs = (void*)*argp ;
#if 1
  log_debug("table: %s, rno: %s, k: %s, v: %s\n",
            targs->table,targs->rowno,targs->k,targs->v);
#endif

#if 1
  int rc = 0;
  do {
    rc = 0;
    // try put
    if (!g_procInfo.hbase.clt.client || hclient_put(&g_procInfo.hbase.clt,
         targs->table,targs->rowno,targs->k,targs->v)) {

      log_debug("retrying put...\n");
      hclient_release(&g_procInfo.hbase.clt);
      hclient_init(&g_procInfo.hbase.clt,g_procInfo.hbase.host,
                   g_procInfo.hbase.port);
      rc = -1;
    }
  } while (rc<0) ;
#endif

	return &result;
}

static
void proc_close(Network_t net, connection_t pconn)
{
  net->unreg_all(net,pconn);

  close(pconn->fd);
}

static
struct module_struct_s g_module = {

  .name = "hsyncd processor",

  .id = -1,

  .dyn_handle = NULL,

  .ssl = false,

  .opts[normal_l4] = {
    .rx = proc_dummy_l4_rx,
    .tx = proc_dummy_l4_tx,
    //.close= proc_close,
  },

  .opts[inbound_l5] = {
    .rx = proc_rx,
    .tx = proc_tx,
    .init = proc_init,
    .release = proc_release,
    .close= proc_close,
  },
} ;

static
int proc_init(Network_t net)
{
  net->reg_local(net,g_procInfo.svc_fd,THIS_MODULE->id);

  return 0;
}

int proc_main(const char *host, int port, int vernum)
{
  g_procInfo.local_vernum = vernum ;

  if (rpc_svc_init(vernum,&g_procInfo.svc_fd,proc_rpc_rx)) {
    log_error("processor %d vernum %d rpc_svc_init fail!\n",
        getpid(),vernum);
    exit(-1);
  }

  strncpy(g_procInfo.hbase.host,host,sizeof(g_procInfo.hbase.host));

  g_procInfo.hbase.port = port ;

  // client connection to hbase
  if (hclient_init(&g_procInfo.hbase.clt,host,port)) {
    log_error("init hclient(server: %s:%d) fail!\n",host,port);
    //return -1;
  }

  register_module(THIS_MODULE);

  return 0;
}

