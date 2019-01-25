
#ifndef __HTTP_UTILS_H__
#define __HTTP_UTILS_H__


extern size_t get_http_hdr_size(char *inb, size_t sz_in);

extern int get_http_hdr_field_int(char *inb, size_t sz_in, const char *field, 
                                  const char *endstr);

extern int get_http_hdr_field_str(char *inb, size_t sz_in, const char *field, 
                                  const char *endstr, char *outb, size_t *sz_out);

extern size_t get_http_body_size(char *inb, size_t sz_in);

extern char* get_http_body_ptr(char *inb, size_t sz_in);

#endif /* __HTTP_UTILS_H__*/
