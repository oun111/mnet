#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "http_utils.h"
#include "connection.h"
#include "kernel.h"
#include "tree_map.h"
#include "myrbtree.h"
#include "log.h"
#include "pay_data.h"
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
    return -1;
  }

  pf = strstr(inb,field);
  if (!pf) {
    return -1;
  }

  fend = strstr(pf,endstr);
  vlen = fend-pf-strlen(field) ;
  strncpy(buf,pf+strlen(field),vlen);
  buf[vlen] = '\0';

  return atoi(buf);
}

size_t get_http_body_size(char *inb, size_t sz_in)
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

dbuffer_t create_json_params(tree_map_t pay_params)
{
  jsonKV_t *pr = 0;
  dbuffer_t strParams = 0;


  drop_tree_map_item(pay_params,REQ_URL,strlen(REQ_URL));
  drop_tree_map_item(pay_params,REQ_PORT,strlen(REQ_PORT));
  drop_tree_map_item(pay_params,PARAM_TYPE,strlen(PARAM_TYPE));

  pr = jsons_parse_tree_map(pay_params);
  strParams = alloc_default_dbuffer();

  jsons_toString(pr,&strParams);
  jsons_release(pr);

  return strParams;
}

dbuffer_t create_html_params(tree_map_t pay_params)
{
  dbuffer_t strParams = alloc_default_dbuffer();
  tm_item_t pos, n ;
  size_t ln = 0L;


  MY_RBTREE_PREORDER_FOR_EACH_ENTRY_SAFE(pos,n,&pay_params->u.root,node) {

    if (!strcmp(pos->key,REQ_URL) || !strcmp(pos->key,REQ_PORT) ||
        !strcmp(pos->key,PARAM_TYPE)) {
      continue ;
    }

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

int create_http_post_req(connection_t pconn, int param_type, tree_map_t pay_params)
{
  //char hdr[1024] = "";
  dbuffer_t strParams = 0;


  if (param_type==pt_html) {
    strParams = create_html_params(pay_params);
  }
  else if (param_type==pt_json) {
    strParams = create_json_params(pay_params);
  }
  else {
    log_error("unknown param type %d\n",param_type);
    return -1;
  }


  drop_dbuffer(strParams);

  // http header
  return 0;
}

