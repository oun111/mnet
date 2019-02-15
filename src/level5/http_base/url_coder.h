#ifndef __URL_CODER_H__
#define __URL_CODER_H__


extern int url_encode(char *inb, size_t sz_in, dbuffer_t *outb);

extern int url_decode(char *inb, size_t sz_in, dbuffer_t *outb);

#endif /* __URL_CODER_H__*/
