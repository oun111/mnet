#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include "log.h"

static int do_sha1_digest(unsigned char *digest, ...)
{
  va_list args;
  SHA_CTX ctx;
  const unsigned char *str;

  if (!SHA1_Init(&ctx)) {
    log_error("can't init sha\n");
    return -1;
  }
  va_start(args, digest);
  for (str= va_arg(args, const unsigned char*); str; 
       str= va_arg(args, const unsigned char*)) {
    SHA1_Update(&ctx, str, va_arg(args, size_t));
  }
  va_end(args);
  SHA1_Final(digest,&ctx);
  OPENSSL_cleanse(&ctx,sizeof(ctx));
  return 0;
}

/* copy from $(mysql source)/sql/password.c */
static void do_encrypt(unsigned char *to, const unsigned char *s1, 
  const unsigned char *s2, unsigned char len)
{
  const unsigned char *s1_end= s1 + len;

  while (s1 < s1_end)
    *to++= *s1++ ^ *s2++;
}

int gen_auth_data(char *scramble, size_t sc_len, 
  const char *passwd, unsigned char *out)
{
  unsigned char hash_stage1[SHA_DIGEST_LENGTH],
    hash_stage2[SHA_DIGEST_LENGTH];

  /* generate hash on password */
  if (do_sha1_digest(hash_stage1,passwd,strlen(passwd),0)) {
    log_error("digest on password #1 failed\n");
    return -1;
  }
  if (do_sha1_digest(hash_stage2,hash_stage1,SHA_DIGEST_LENGTH,0)) {
    log_error("digest on password #2 failed\n");
    return -1;
  }
  /* generate hash on 'password' + 'auth data' */
  if (do_sha1_digest(out,scramble,sc_len,
     hash_stage2,SHA_DIGEST_LENGTH,0)) {
    log_error("digest on password failed\n");
    return -1;
  }
  /* encrypt the 'out' buffer */
  do_encrypt(out,out,hash_stage1,SHA_DIGEST_LENGTH);
  return 0;
}

unsigned long str2id(char *s,size_t sz)
{
  unsigned long ret=0;
  unsigned int i=0;
  char *p = s;
  char *pEnd = s+sz;

  if (!p) 
    return 0;
  for (;p&&*p&&p<pEnd;p++) 
    ret += (*p^i) ;
  ret ^= (32&strlen(s)) ; 
  return ret ;
}

