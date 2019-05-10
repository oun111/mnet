#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include "dbuffer.h"
#include "md.h"


#if 0
int do_md_sign(const char *inb, size_t sz_in, int type, const char *hkey, 
               size_t sz_key, char **outb, size_t *sz_out)
{
  /* Returned to caller */
  int ret = -1, rc = 0;
  size_t slen = 0L;
  EVP_MD_CTX* ctx = EVP_MD_CTX_create();
  const char *mdname = type==MD_MD5?"MD5":type==MD_SHA256?"SHA256":NULL ;
  EVP_PKEY *pkey = 0;


  if (!mdname) {
    printf("unknown message digest type '%d'\n",type);
    return -1;
  }

  if(ctx == NULL) {
      printf("EVP_MD_CTX_create failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  const EVP_MD* md = EVP_get_digestbyname(mdname);
  if(md == NULL) {
      printf("EVP_get_digestbyname failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  rc = EVP_DigestInit_ex(ctx, md, NULL);
  if(rc != 1) {
      printf("EVP_DigestInit_ex failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }

  if (hkey) {
    pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, (const unsigned char*)hkey, sz_key);
    if(pkey == NULL) {
        printf("EVP_PKEY_new_mac_key failed, error 0x%lx\n", ERR_get_error());
        goto __done;
    }
  }
  
  rc = EVP_DigestSignInit(ctx, NULL, md, NULL, pkey);
  if(rc != 1) {
      printf("EVP_DigestSignInit failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  rc = EVP_DigestSignUpdate(ctx, inb, sz_in);
  if(rc != 1) {
      printf("EVP_DigestSignUpdate failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  rc = EVP_DigestSignFinal(ctx, NULL, &slen);
  if(rc != 1) {
      printf("EVP_DigestSignFinal failed (1), error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  if(slen<=0) {
      printf("EVP_DigestSignFinal failed (2), error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  *outb = OPENSSL_malloc(slen);
  if(*outb == NULL) {
      printf("OPENSSL_malloc failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  *sz_out = slen;
  rc = EVP_DigestSignFinal(ctx, (unsigned char*)*outb, sz_out);
  if(rc != 1) {
      printf("EVP_DigestSignFinal failed (3), return code %d, error 0x%lx\n", rc, ERR_get_error());
      goto __done; /* failed */
  }
  
  if(slen != *sz_out) {
      printf("EVP_DigestSignFinal failed, mismatched signature sizes %ld, %ld", slen, *sz_out);
      goto __done; /* failed */
  }
  
  ret = 0;
  
__done:
  if(ctx) {
    EVP_MD_CTX_destroy(ctx);
  }
  if (pkey) {
    EVP_PKEY_free(pkey);
  }
  
  return ret;
}
#endif 

/*
 * message digest sign
 */
int do_md_sign(const char *inb, size_t sz_in, int type, dbuffer_t *outb)
{
  /* Returned to caller */
  int ret = -1, rc = 0;
  unsigned char res[EVP_MAX_MD_SIZE];
  unsigned int szres = 0;
  EVP_MD_CTX* ctx = EVP_MD_CTX_create();
  const char *mdname = type==MD_MD5?"MD5":
                       type==MD_SHA1?"SHA1":
                       type==MD_SHA224?"SHA224":
                       type==MD_SHA256?"SHA256":
                       type==MD_SHA384?"SHA384":
                       type==MD_SHA512?"SHA512":
                       NULL ;


  if (!mdname) {
    printf("unknown message digest type '%d'\n",type);
    return -1;
  }

  if(ctx == NULL) {
      printf("EVP_MD_CTX_create failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  const EVP_MD* md = EVP_get_digestbyname(mdname);
  if(md == NULL) {
      printf("EVP_get_digestbyname failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }
  
  rc = EVP_DigestInit_ex(ctx, md, NULL);
  if(rc != 1) {
      printf("EVP_DigestInit_ex failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }

  rc = EVP_DigestUpdate(ctx,inb,sz_in);
  if(rc != 1) {
      printf("EVP_DigestUpdate failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }

  rc = EVP_DigestFinal_ex(ctx, res, &szres);
  if(rc != 1) {
      printf("EVP_DigestFinal_ex failed, error 0x%lx\n", ERR_get_error());
      goto __done; /* failed */
  }

  for (unsigned int i=0;i<szres;i++) {
    char tmp[5] = "";
    sprintf(tmp,"%02x",res[i]&0xff);
    append_dbuf_str(*outb,tmp);
  }
  
  ret = 0;
  
__done:
  if(ctx) {
    EVP_MD_CTX_destroy(ctx);
  }
  
  return ret;
}

/*
 * message digest verify
 */
bool do_md_verify(const char *inb, size_t sz_in, const char *strsign, 
                  size_t sz_sign, int type)
{
  bool result = false;
  dbuffer_t outb = alloc_default_dbuffer();


  if (do_md_sign(inb,sz_in,type,&outb)) {
    goto __done ;
  }

  if (!strcasecmp(strsign,outb)) {
    result = true;
  }

__done:
  drop_dbuffer(outb);

  return result;
}

#if TEST_CASES==1
void test_md()
{
  char strsign[] = "88c6ee3e7e8008d98c35a8325ccf0d23";
  char inb[] = "123456789";
  int ret = 0;
  dbuffer_t outb = alloc_default_dbuffer();

  ret = do_md_sign(inb,strlen(inb),MD_MD5,&outb);
  if (!ret) {
    printf("res: %s\n",outb);
  }

  printf("verify %s\n",do_md_verify(inb,strlen(inb),
         strsign,strlen(strsign),MD_MD5)?"ok":"fail");

  drop_dbuffer(outb);

  exit(0);
}
#endif

