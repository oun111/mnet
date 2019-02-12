#include "instance.h"
#include "action.h"
#include "config.h"
#include "http_svr.h"


void pay_svr_module_init(int argc, char *argv[])
{
  __http_svr_module_init(argc,argv);
}

void pay_svr_module_exit()
{
  __http_svr_module_exit();
}

