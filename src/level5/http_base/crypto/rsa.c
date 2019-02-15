#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "dbuffer.h"


static
RSA* gen_rsa0(const char *key_path, bool ispriv)
{
  FILE *fp = fopen(key_path,"r");
  RSA *rsa = NULL ;
  BIO *bio = 0;
  char key[4096]="";


  if (!fp) {
    printf("fail to open '%s'\n",key_path);
    return NULL ;
  }

  fread(key,4096,1,fp);


  bio = BIO_new_mem_buf(key,-1);
  if (!bio) {
    printf("fail to create bio\n");
    fclose(fp);
    return NULL ;
  }

  if (ispriv) 
    rsa = PEM_read_bio_RSAPrivateKey(bio,&rsa,NULL,NULL);
  else 
    rsa = PEM_read_bio_RSA_PUBKEY(bio,&rsa,NULL,NULL);

  BIO_free(bio);
  fclose(fp);

  return rsa ;
}

static
RSA* gen_rsa(const char *key_path, bool ispriv)
{
  FILE *fp = fopen(key_path,"r");
  RSA *rsa = NULL ;


  if (!fp) {
    printf("fail to open '%s'\n",key_path);
    return NULL ;
  }

  if (ispriv) 
    rsa = PEM_read_RSAPrivateKey(fp,&rsa,NULL,NULL);
  else 
    rsa = PEM_read_RSA_PUBKEY(fp,&rsa,NULL,NULL);

  fclose(fp);

  return rsa ;
}

static
int do_rsa_sign(const char *key_path, const char *plain_text, 
                unsigned char **res, unsigned int *sz_out, bool sign)
{
  RSA *rsa = gen_rsa(key_path,sign);
  int ret = 0;


  if (!rsa) {
    ERR_print_errors_fp(stdout);
    return -1;
  }

  if (sign==true) {
    *sz_out = RSA_size(rsa);
    *res = malloc(*sz_out);

    ret = RSA_sign(NID_sha256WithRSAEncryption,(unsigned char*)plain_text,
                   strlen(plain_text),*res,sz_out,rsa);
  }
  else {
    ret = RSA_verify(NID_sha256WithRSAEncryption,(unsigned char*)plain_text,
                     strlen(plain_text),*res,*sz_out,rsa);
  }

  if (ret!=1) {
    ERR_print_errors_fp(stdout);
    RSA_free(rsa);
    return -1;
  }

  RSA_free(rsa);

  return ret;
}

int rsa_private_sign(const char *priv_key_path, const char *plain_text, 
                     unsigned char **res, unsigned int *sz_out)
{
  return do_rsa_sign(priv_key_path,plain_text,res,sz_out,true);
}

int rsa_public_verify(const char *pub_key_path, const char *plain_text,
                      unsigned char *res, unsigned int sz_out)
{
  return do_rsa_sign(pub_key_path,plain_text,&res,&sz_out,false);
}

#if TEST_CASES==1
void test_crypto()
{
  unsigned char *res = 0;
  unsigned int sz_out = 0;
  const char privkeypath[] = "/home/user1/work/mnet/conf/rsa_private_key.pem";
  const char pubkeypath[] = "/home/user1/work/mnet/conf/rsa_public_key.pem";
  int ret=  0;


  ret = rsa_private_sign(privkeypath,"123",&res,&sz_out) ;
  printf("sign %sok!\n",ret>0?"":"NOT ");

  ret = rsa_public_verify(pubkeypath,"123",res,sz_out); 
  printf("result %smatches!\n",ret>0?"":"NOT ");

  if (res)
    free(res);
}
#endif

