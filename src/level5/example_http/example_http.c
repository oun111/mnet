#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <getopt.h>
#include "connection.h"
#include "dbuffer.h"
#include "log.h"
#include "file_cache.h"
#include "instance.h"
#include "socket.h"
#include "module.h"


const char normalHdr[] = "HTTP/1.1 %s\r\n"
                         "Server: Apache-Coyote/1.1\r\n"
                         "Content-Type: text/html;charset=GBK\r\n"
                         "Content-Length:%zu      \r\n"
                         "Date: %s\r\n\r\n";


enum page_types {
  t_begin,
  t_404,
  t_max
} ;

struct example_http_conf {
  int fd;
  char host[32];
  int port;
  char home[PATH_MAX];

  struct static_page_s {
    const char *name;
    dbuffer_t buff ;
  } static_pages[t_max];

} g_example_http_conf = {
  .host = "127.0.0.1",
  .port = 4321,
  .fd = -1,
  .home = "/home/user1/work/mnet/src/level5/example_http/web/",

  .static_pages[t_404] = { .name = "404.html", },
};



int example_http_init(Network_t net);

static int parse_cmd_line(int argc, char *argv[]);



static char* example_http_get_contents(dbuffer_t req, const char end[], char *end_ch)
{
  char *str = strstr(req,end);

  if (!str)
    return NULL;

  str += strlen(end);
  *end_ch = *str ;
  *str = '\0' ;

  return str;
}

static 
int example_http_do_error(connection_t pconn)
{
  char *bodyPage = (char*)g_example_http_conf.static_pages[t_404].buff ;
  char hdr[256] = "";
  time_t t = time(NULL);
  char tb[64];

  ctime_r(&t,tb);
  snprintf(hdr,256,normalHdr,"404 Not Found",strlen(bodyPage),/*"0000"*/tb);

  pconn->txb = write_dbuffer(pconn->txb,hdr,strlen(hdr));
  pconn->txb = append_dbuffer(pconn->txb,bodyPage,strlen(bodyPage));

  return 0;
}

static 
int example_http_list_all_files(connection_t pconn,char *dir,time_t t)
{
  char hdr[256];
  char listBegin[] = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">"
                     "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">"
                     "<title>Directory listing for /</title></head><body><h1>Directory listing for /</h1>"
                     "<hr><ul>";
  char listEnd[]   = "</ul><hr></body></html>";
  dbuffer_t rpos = 0;
  size_t dlen = 0L;
  char tb[64];


  ctime_r(&t,tb);
  snprintf(hdr,256,normalHdr,"200 Ok",0L,/*"0000"*/tb);
  pconn->txb = write_dbuffer(pconn->txb,hdr,strlen(hdr));

  pconn->txb = append_dbuffer(pconn->txb,listBegin,strlen(listBegin));
  dlen += strlen(listBegin);

  {
    DIR *dp = opendir(dir) ;
    struct dirent *dir = 0;

    while ((dir=readdir(dp))) {
      char filetbl[600] = "<li><a href=\"";

      if (0&& dir->d_type==DT_DIR && dir->d_name[0]=='.')
        continue ;

      strcat(filetbl,dir->d_name);
      strcat(filetbl,"\">");
      strcat(filetbl,dir->d_name);
      if (dir->d_type==DT_DIR)
        strcat(filetbl,"/");
      strcat(filetbl,"</a></li>");
      dlen += strlen(filetbl);

      pconn->txb = append_dbuffer(pconn->txb,filetbl,strlen(filetbl));
    }

    closedir(dp);
  }

  pconn->txb = append_dbuffer(pconn->txb,listEnd,strlen(listEnd));
  dlen += strlen(listEnd);

  /* fill back the http body size */
  rpos = strstr(pconn->txb,"Content-Length:");
  if (rpos) {
    sprintf(rpos+strlen("Content-Length:"),"%zu",dlen);
  }

  return 0;
}

static 
int example_http_change_to_default_path()
{
  char *path = g_example_http_conf.home ;

  chdir(path);
  log_debug("change dir to %s\n",path);

  return 0;
}

static 
int example_http_do_post(connection_t pconn, dbuffer_t req, size_t len)
{
  char hdr[256];
  time_t t = time(NULL);
  char post_target[PATH_MAX] = "";
  /* XXX: '/' should be existing */
  dbuffer_t gt = strstr(req,"/");
  dbuffer_t end1 = strstr(gt," ");
  struct stat st ;
  char tb[64];


  ctime_r(&t,tb);

  strncat(post_target,*(gt+1)==' '?gt:(gt+1),end1-gt);
  log_debug("post target: %s\n",post_target);


  /* 
   * get all files under current directroy 
   */
  if (!strncmp(post_target,"listall",7) || !strcmp(post_target,"/")) {

    example_http_change_to_default_path();

    example_http_list_all_files(pconn,".",t);

    return 0;
  }

  /*
   * check the 'GET' file
   */
  getcwd(post_target,PATH_MAX);
  strncat(post_target,gt,end1-gt);
  log_debug("GET/POST file: %s\n",post_target);

  if (stat(post_target,&st)==-1) {
    log_error("access file %s error: %s\n",post_target,strerror(errno));
    return -1;
  } else {
    if (S_ISDIR(st.st_mode)) {
      chdir(post_target);
      example_http_list_all_files(pconn,post_target,t);
    }
    else {
      snprintf(hdr,256,normalHdr,"200 Ok",st.st_size,/*"0000"*/tb);
      pconn->txb = write_dbuffer(pconn->txb,hdr,strlen(hdr));

      load_file(post_target,&pconn->txb);
    }

  }


  return 0;
}

static void example_http_print_debug_req(dbuffer_t req, size_t len, int res)
{
  char c = 0;
  char *str = example_http_get_contents(req,"\r\n",&c);

  if (str) {
    log_debug("%s...(size %zu) - %s\n",req,len,res?"ok":"fail");
    *str = c;
  }
}

int example_http_rx(Network_t net, connection_t pconn)
{
  size_t datalen = dbuffer_data_size(pconn->rxb);
  dbuffer_t subreq = 0, next = dbuffer_ptr(pconn->rxb,0);
  int isOk = 1;


  if (datalen==0)
    return 0;

  log_debug("\n\n===========\noriginal req: %s===========\n\n",next);

  while (isOk>=0 && next && ((subreq=strcasestr(next,"POST")) || 
        (subreq=strcasestr(next,"GET")))) {

    char c = 0;
    dbuffer_t end = example_http_get_contents(subreq,"\r\n\r\n",&c);

    /* get a whole sub request */
    if (end) {

      size_t ln = end-subreq;

      isOk = !example_http_do_post(pconn,subreq,ln);
      end[0] = c ;

      next = end ;

      example_http_print_debug_req(subreq,ln,isOk);
    } 
    else {
      isOk = -1;
    }

    if (isOk<=0) 
      example_http_do_error(pconn);

    pconn->l5opt.tx(net,pconn);
  }

  /* REMEMBER TO update the read pointer of rx buffer */
  dbuffer_lseek(pconn->rxb,datalen,SEEK_CUR,0);

  return 0;
}

int example_http_tx(Network_t net, connection_t pconn)
{
  pconn->l4opt.tx(net,pconn);

  return 0;
}

static
int load_static_pages()
{
  char path[PATH_MAX] = "";


  for (int i=t_begin+1;i<t_max;i++) {
    g_example_http_conf.static_pages[i].buff = 
      realloc_dbuffer(g_example_http_conf.static_pages[i].buff,0L);
    
    strcpy(path,g_example_http_conf.home) ;
    strcat(path,g_example_http_conf.static_pages[i].name) ;

    if (load_file(path,&g_example_http_conf.static_pages[i].buff)) {
      return -1;
    }
  }

  return 0;
}

int example_http_pre_init(int argc, char *argv[])
{
  char *host = NULL;
  int port = 0 ;


  if (parse_cmd_line(argc,argv))
    return 1;

  host = g_example_http_conf.host;
  port = g_example_http_conf.port ;

  g_example_http_conf.fd = new_tcp_svr(__net_atoi(host),port);

  log_debug("bound to %s:%d\n",host,port);

  return 0;
}

void example_http_release()
{
  close(g_example_http_conf.fd);

  for (int i=t_begin+1;i<t_max;i++) 
    drop_dbuffer(g_example_http_conf.static_pages[i].buff);
}


static 
struct module_struct_s g_module = {

  .name = "example http",

  .id = -1,

  .dyn_handle = NULL,
  
  .ssl = false,

  .opts[inbound_l5] = {
    .rx = example_http_rx,
    .tx = example_http_tx,
    .init = example_http_init,
    .release = example_http_release,
  },
};


static
int parse_cmd_line(int argc, char *argv[])
{
  char *ptr = 0;

  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i],"-H") && i+1<argc) {
      ptr = argv[i+1];
      strcpy(g_example_http_conf.host,ptr);
    }
    else if (!strcmp(argv[i],"-P") && i+1<argc) {
      ptr = argv[i+1];
      g_example_http_conf.port = atoi(ptr);
    }
    else if (!strcmp(argv[i],"-o") && i+1<argc) {
      ptr = argv[i+1];
      strcpy(g_example_http_conf.home,ptr);
    }
    else if (!strcmp(argv[i],"-h")) {
      printf("module '%s' help message\n",THIS_MODULE->name);
      printf("-H: bind address\n");
      printf("-P: bind port\n");
      printf("-o: home directory\n");
      return 1;
    }
  }
  return 0;
}

int example_http_init(Network_t net)
{
  load_static_pages();

  net->reg_local(net,g_example_http_conf.fd,THIS_MODULE->id);

  example_http_change_to_default_path();

  log_info("done!\n");

  return 0;
}

static 
void example_http_dump_params()
{
  char *host = (char*)g_example_http_conf.host;
  int port = g_example_http_conf.port;
  char *home = g_example_http_conf.home ;


  log_info("module '%s' param list: =================\n",THIS_MODULE->name);
  log_info("bound to: %s:%d\n",host,port);
  log_info("home: %s\n",home);
  log_info("module param list end =================\n");
}

void example_http_module_init(int argc, char *argv[])
{
  if (example_http_pre_init(argc,argv))
    return ;

  example_http_dump_params();

  register_module(&g_module);
}
//MODULE_INIT(example_http_module_init)

void example_http_module_exit()
{
  example_http_release();
}
//MODULE_EXIT(example_http_module_exit)

