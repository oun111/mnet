#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"


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

