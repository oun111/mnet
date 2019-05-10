#ifndef __MD_H__
#define __MD_H__


enum md_types {
  MD_MD5,
  //MD_HMACMD5,
  MD_SHA1,
  MD_SHA224,
  MD_SHA256,
  MD_SHA384,
  MD_SHA512,
};



int do_md_sign(const char *inb, size_t sz_in, int type, dbuffer_t *outb);

extern bool do_md_verify(const char *inb, size_t sz_in, const char *strsign, 
                         size_t sz_sign, int type);

#endif /* __MD_H__*/
