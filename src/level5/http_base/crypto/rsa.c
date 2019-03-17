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

#if 0
//static
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
#endif

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

#if 0
//static
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
    *res = OPENSSL_malloc(*sz_out);

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
#else
/**
 * 
 * Another way to do RSA sign/verify, I refer to:
 *
 *  https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying
 *
 */
static
int do_rsa_sign(const char *key_path, const char *plain_text, 
                 unsigned char **sig, size_t *sz_out, bool sign)
{
  RSA *rsa = gen_rsa(key_path,sign);
  int ret = -1;
  size_t slen = 0L;
  EVP_PKEY *pkey = 0;
  EVP_MD *md = 0;
  EVP_MD_CTX *ctx = 0;


  if (!rsa) {
    ERR_print_errors_fp(stdout);
    goto __done ;
  }

  pkey = EVP_PKEY_new();
  if (!pkey) {
    printf("EVP_PKEY_new fail: %lu\n", ERR_get_error());
    goto __done ;
  }

  md = (EVP_MD*)EVP_get_digestbyname("SHA256");
  if (!md) {
    printf("EVP_get_digestbyname fail: %lu\n", ERR_get_error());
    goto __done ;
  }

  ctx = EVP_MD_CTX_create();
  if (!ctx) {
    printf("EVP_MD_CTX_create fail: \n");
    ERR_print_errors_fp(stdout);
    goto __done ;
  }

  ret = EVP_PKEY_assign_RSA(pkey,rsa);
  if (ret!=1) {
    printf("EVP_PKEY_assign_RSA fail: \n");
    ERR_print_errors_fp(stdout);
    goto __done ;
  }

  ret = EVP_DigestInit_ex(ctx, md, NULL);
  if (ret!=1) {
    printf("EVP_DigestInit_ex fail: \n");
    ERR_print_errors_fp(stdout);
    goto __done ;
  }

  // sign it
  if (sign==true) {

    ret = EVP_DigestSignInit(ctx, NULL, md, NULL, pkey);
    if (ret!=1) {
      printf("EVP_DigestSignInit fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    ret = EVP_DigestSignUpdate(ctx,plain_text,strlen(plain_text));
    if (ret!=1) {
      printf("EVP_DigestSignUpdate fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    ret = EVP_DigestSignFinal(ctx, NULL, &slen);
    if (ret!=1) {
      printf("EVP_DigestSignFinal fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    if (slen<=0) {
      printf("digest size error\n");
      goto __done ;
    }

    *sig = OPENSSL_malloc(slen);
    if (!*sig) {
      printf("OPENSSL_malloc fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    *sz_out = slen ;
    ret = EVP_DigestSignFinal(ctx, *sig, sz_out);
    if (ret!=1) {
      printf("EVP_DigestSignFinal fail 1: ");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    if (*sz_out!=slen) {
      printf("digest size not match: %lu\n", ERR_get_error());
      goto __done ;
    }
  }
  // verify it
  else {
    ret = EVP_DigestVerifyInit(ctx, NULL, md, NULL, pkey);
    if (ret!=1) {
      printf("EVP_DigestVerifyInit fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    ret = EVP_DigestVerifyUpdate(ctx,plain_text,strlen(plain_text));
    if (ret!=1) {
      printf("EVP_DigestVerifyUpdate fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }

    ERR_clear_error();

    ret = EVP_DigestVerifyFinal(ctx, *sig, *sz_out);
    if (ret!=1) {
      printf("EVP_DigestVerifyFinal fail: \n");
      ERR_print_errors_fp(stdout);
      goto __done ;
    }
  }

  ret = 1;

__done:
#if 0
  if (pkey)
    EVP_PKEY_free(pkey);
#endif
  if (rsa)
    RSA_free(rsa);
  if (ctx)
    EVP_MD_CTX_destroy(ctx);

  return ret;
}
#endif

int rsa_private_sign(const char *priv_key_path, const char *plain_text, 
                     unsigned char **res, size_t *sz_out)
{
  //return do_rsa_sign(priv_key_path,plain_text,res,sz_out,true);
  return do_rsa_sign(priv_key_path,plain_text,res,(size_t*)sz_out,true);
}

int rsa_public_verify(const char *pub_key_path, const char *plain_text,
                      unsigned char *res, size_t sz_out)
{
  //return do_rsa_sign(pub_key_path,plain_text,&res,&sz_out,false);
  return do_rsa_sign(pub_key_path,plain_text,&res,(size_t*)&sz_out,false);
}

#if TEST_CASES==1
#include "base64.h"
void test_crypto()
{
  unsigned char *res = 0;
  size_t sz_out = 0;
  const char privkeypath[] = "/home/user1/work/mnet/src/level5/pay_svr/rsa_private_key.pem";
  const char pubkeypath[] = "/home/user1/work/mnet/src/level5/pay_svr/rsa_public_key.pem";
  int ret=  0;
  //char *signstr = "app_id=2018102961967184&biz_content={\\\"total_amount\\\":0.01,\\\"timeout_express\\\":\\\"3m\\\",\\\"subject\\\":\\\"%E5%95%86%E5%9F%8E1\\\",\\\"product_code\\\":\\\"QUICK_WAP_PAY\\\",\\\"out_trade_no\\\":\\\"alp_id_0000000001\\\",\\\"body\\\":\\\"%E8%B4%A7%E6%AC%BE\\\"}&charset=utf-8&format=JSON&method=alipay.trade.wap.pay&notify_url=https://127.0.0.1/alipay/notify&return_url=http://127.0.0.1/return/index&sign_type=RSA2&timestamp=2019-3-16 21:41:2&version=1.0";
  char *signstr = "app_id=2018102961967184&sign_type=RSA2";


  ret = rsa_private_sign(privkeypath,signstr,&res,&sz_out) ;
  printf("\ndigest size: %zu, sign %sok!\n",strlen(signstr),ret>0?"":"NOT ");

  {
    unsigned char final_res[1024] = "";
    int sz_res = 1024;
    base64_encode(res,sz_out,final_res,&sz_res) ;
    printf("digest: %s\n",final_res);
  }

#if 1
  ret = rsa_public_verify(pubkeypath,signstr,res,sz_out); 
  printf("result %smatches!\n",ret>0?"":"NOT ");
#else
  (void)pubkeypath;
#endif

  if (res)
    OPENSSL_free(res);
}
#endif

