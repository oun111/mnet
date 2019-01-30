#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "http_utils.h"
#include "connection.h"
#include "kernel.h"
#include "tree_map.h"
#include "myrbtree.h"
#include "log.h"
#include "jsons.h"


size_t get_http_hdr_size(char *inb, size_t sz_in)
{
  char *p = strstr(inb,"\r\n\r\n");

  return p?(p-inb+4):0;
}

int get_http_hdr_field_str(char *inb, size_t sz_in, const char *field, 
                           const char *endstr, char *outb, size_t *sz_out)
{
  const size_t sz_hdr = get_http_hdr_size(inb,sz_in);
  char *pf = 0, *fend = 0;
  size_t vlen = 0L;


  if (!sz_hdr) {
    return -1;
  }

  pf = strstr(inb,field);
  if (!pf) {
    log_error("cant find field '%s'\n",field);
    return -1;
  }

  fend = strstr(pf,endstr);
  vlen = fend-pf-strlen(field) ;
  if (vlen>*sz_out) {
    log_error("res size %zu > %zu\n",vlen,*sz_out);
    return -1;
  }

  if (vlen==0L) {
    outb[0] = '\0';
    *sz_out = 0L;
    return 0;
  }

  strncpy(outb,pf+strlen(field),vlen);
  outb[vlen] = '\0';
  *sz_out = vlen ;

  return 0;
}

int get_http_hdr_field_int(char *inb, size_t sz_in, const char *field, const char *endstr)
{
  const size_t sz_hdr = get_http_hdr_size(inb,sz_in);
  char *pf = 0, *fend = 0;
  char buf[32] = "";
  size_t vlen = 0L;


  if (!sz_hdr) {
    return 0;
  }

  pf = strstr(inb,field);
  if (!pf) {
    return 0;
  }

  fend = strstr(pf,endstr);
  vlen = fend-pf-strlen(field) ;
  strncpy(buf,pf+strlen(field),vlen);
  buf[vlen] = '\0';

  return atoi(buf);
}

ssize_t get_http_body_size(char *inb, size_t sz_in)
{
  return get_http_hdr_field_int(inb,sz_in,"Content-Length:","\r\n");
}

char* get_http_body_ptr(char *inb, size_t sz_in)
{
  size_t szhdr = get_http_hdr_size(inb,sz_in);

  if (szhdr==0L) {
    return NULL;
  }

  return inb+szhdr ;
}

dbuffer_t create_json_params(tree_map_t pay_data)
{
  jsonKV_t *pr = 0;
  dbuffer_t strParams = 0;


  pr = jsons_parse_tree_map(pay_data);
  strParams = alloc_default_dbuffer();

  jsons_toString(pr,&strParams);
  jsons_release(pr);

  return strParams;
}

dbuffer_t create_html_params(tree_map_t pay_data)
{
  dbuffer_t strParams = alloc_default_dbuffer();
  tm_item_t pos, n ;
  size_t ln = 0L;


  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&pay_data->u.root,node) {

    // construct 'key=value'
    ln = dbuffer_data_size(pos->key); 
    strParams = append_dbuffer(strParams,pos->key,ln);

    strParams = append_dbuffer(strParams,"=",1);

    ln = dbuffer_data_size(pos->val); 
    strParams = append_dbuffer(strParams,pos->val,ln);

    strParams = append_dbuffer(strParams,"&",1);
  }

  return strParams ;
}

int create_http_post_req(dbuffer_t *inb, const char *url, 
                         int param_type, tree_map_t pay_data)
{
  char hdr[1024] = "";
  dbuffer_t strParams = 0;
  char host[128] = "", uri[256]="";
  const char hdrFmt[] = "HTTP/1.1 %s\r\n"
                           "User-Agent: pay-svr/0.1\r\n"
                           "Content-Type: %s\r\n"
                           "Content-Length: %zu\r\n"
                           "Host: %s\r\n\r\n";


  if (param_type==pt_html) {
    strParams = create_html_params(pay_data);
  }
  else {
    strParams = create_json_params(pay_data);
  }

  // http header
  parse_http_url(url,host,128,uri,256);
  snprintf(hdr,sizeof(hdr),hdrFmt,uri,param_type==pt_html?"text/xml":"text/json",
           dbuffer_data_size(strParams),host);
  log_debug("post header: %s\n",hdr);

  // attach whole body
  *inb = append_dbuffer(*inb,hdr,strlen(hdr));
  *inb = append_dbuffer(*inb,strParams,dbuffer_data_size(strParams));


  drop_dbuffer(strParams);

  return 0;
}

int parse_http_url(const char *url, char *host, size_t szhost,
                   char *uri, size_t szuri)
{
  char *phost = strstr(url,"://"), *hend = 0;
  char *urlend = (char*)url + strlen(url);
  size_t ln = 0L;


  phost = phost?(phost+3):(char*)url ;
  hend = strchr(phost,'/');

  // host
  ln = hend?(hend-phost):(urlend-phost);
  if (host) {
    strncpy(host,phost,ln<szhost?ln:(szhost-1));
    host[ln] = '\0';
  }

  // uri
  phost += ln ;
  ln = strlen(url)-ln;
  if (uri) {
    if (ln>0) {
      strncpy(uri,phost,ln<szuri?ln:(szuri-1));
      uri[ln] = '\0';
    }
    else {
      uri[0] = '/' ;
      uri[1] = '\0';
    }
  }

  return 0;
}

#if TEST_CASES==1
void test_http_utils()
{
  char url[]=  "www.163.com///";
  char host[32], uri[64];

  parse_http_url(url,host,32,NULL,0L);

  printf("host111: %s, uri: %s\n",host,uri);
}
#endif
