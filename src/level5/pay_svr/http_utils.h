
#ifndef __HTTP_UTILS_H__
#define __HTTP_UTILS_H__

#include "connection.h"
#include "tree_map.h"

enum http_param_types{
  pt_html,
  pt_json
} ;

extern size_t get_http_hdr_size(char *inb, size_t sz_in);

extern int get_http_hdr_field_int(char *inb, size_t sz_in, const char *field, 
                                  const char *endstr);

extern int get_http_hdr_field_str(char *inb, size_t sz_in, const char *field, 
                                  const char *endstr, char *outb, size_t *sz_out);

extern size_t get_http_body_size(char *inb, size_t sz_in);

extern char* get_http_body_ptr(char *inb, size_t sz_in);

extern int create_http_post_req(connection_t, int, tree_map_t);

#endif /* __HTTP_UTILS_H__*/
