
#ifndef __HTTP_UTILS_H__
#define __HTTP_UTILS_H__

#include "dbuffer.h"
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

extern ssize_t get_http_body_size(char *inb, size_t sz_in);

extern char* get_http_body_ptr(char *inb, size_t sz_in);

extern int create_http_post_req(dbuffer_t*, const char*, int, tree_map_t);

extern int create_http_get_req(dbuffer_t*, const char*, int, tree_map_t);

extern int create_http_get_req2(dbuffer_t *inb, const char *wholeurl);

extern int create_browser_redirect_req(dbuffer_t*, const char*, int, tree_map_t);

extern int create_http_normal_res(dbuffer_t*, int, int, const char*);

extern int create_http_normal_res2(dbuffer_t *, int, tree_map_t);

extern int parse_http_url(const char *url, char *host, size_t szhost,
                          int *port, char *uri, size_t szuri, bool *is_ssl);

extern dbuffer_t create_html_params(tree_map_t map);

extern dbuffer_t create_json_params(tree_map_t map);

extern int urlencode_tree_map(tree_map_t map);

extern int uri_to_map(char *strKv, size_t kvLen, tree_map_t entry);

#endif /* __HTTP_UTILS_H__*/
