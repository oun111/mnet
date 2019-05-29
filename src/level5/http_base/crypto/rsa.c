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
#include "crypto.h"

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


/**
 * 
 * Another way to do RSA sign/verify, I refer to:
 *
 *  https://wiki.openssl.org/index.php/EVP_Signing_and_Verifying
 *
 */
static
int __rsa_sign(RSA *rsa, const char *plain_text, 
               unsigned char **sig, size_t *sz_out, bool sign)
{
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

  ret = EVP_PKEY_assign_RSA(pkey,sign?RSAPrivateKey_dup(rsa):RSAPublicKey_dup(rsa));
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
  if (pkey)
    EVP_PKEY_free(pkey);
  if (ctx)
    EVP_MD_CTX_destroy(ctx);

  return ret;
}

static
int do_rsa_sign(const char *key_path, const char *plain_text, 
                 unsigned char **sig, size_t *sz_out, bool sign)
{
  RSA *rsa = gen_rsa(key_path,sign);
  int ret  = __rsa_sign(rsa,plain_text,sig,sz_out,sign);


  if (rsa)
    RSA_free(rsa);

  return ret;
}

static
int do_rsa_sign_fast(rsa_entry_t entry, const char *plain_text, 
                     unsigned char **sig, size_t *sz_out, bool sign)
{
  RSA *rsa = sign?entry->sign_item:entry->versign_item;


  return __rsa_sign(rsa,plain_text,sig,sz_out,sign);
}

int rsa_private_sign(const char *priv_key_path, const char *plain_text, 
                     unsigned char **res, size_t *sz_out)
{
  return do_rsa_sign(priv_key_path,plain_text,res,(size_t*)sz_out,true);
}

int rsa_public_verify(const char *pub_key_path, const char *plain_text,
                      unsigned char *res, size_t sz_out)
{
  return do_rsa_sign(pub_key_path,plain_text,&res,(size_t*)&sz_out,false);
}

int rsa_private_sign1(rsa_entry_t entry, const char *plain_text, 
                      unsigned char **res, size_t *sz_out)
{
  return do_rsa_sign_fast(entry,plain_text,res,(size_t*)sz_out,true);
}

int rsa_public_verify1(rsa_entry_t entry, const char *plain_text,
                      unsigned char *res, size_t sz_out)
{
  return do_rsa_sign_fast(entry,plain_text,&res,(size_t*)&sz_out,false);
}

int init_rsa_entry(rsa_entry_t entry, const char *pub_key_path, 
                   const char *priv_key_path)
{
  entry->sign_item    = gen_rsa(priv_key_path,true);

  entry->versign_item = gen_rsa(pub_key_path,false);

  return 0;
}

int release_rsa_entry(rsa_entry_t entry)
{
  if (entry->sign_item)
    RSA_free(entry->sign_item);

  if (entry->versign_item)
    RSA_free(entry->versign_item);

  return 0;
}

#if TEST_CASES==1
#include "base64.h"
#include "tree_map.h"
#include "http_utils.h"
void test_rsa()
{
  unsigned char *res = 0;
  size_t sz_out = 0;
  const char privkeypath[] = "/home/user1/work/mnet/src/level5/pay_svr/keys/rsa_private_key1.pem";
  const char pubkeypath[] = "/home/user1/work/mnet/src/level5/pay_svr/keys/rsa_public_key1.pem";
  int ret=  0;
  //char *signstr = "app_id=2018102961967184&biz_content={\\\"total_amount\\\":0.01,\\\"timeout_express\\\":\\\"3m\\\",\\\"subject\\\":\\\"%E5%95%86%E5%9F%8E1\\\",\\\"product_code\\\":\\\"QUICK_WAP_PAY\\\",\\\"out_trade_no\\\":\\\"alp_id_0000000001\\\",\\\"body\\\":\\\"%E8%B4%A7%E6%AC%BE\\\"}&charset=utf-8&format=JSON&method=alipay.trade.wap.pay&notify_url=https://127.0.0.1/alipay/notify&return_url=http://127.0.0.1/return/index&sign_type=RSA2&timestamp=2019-3-16 21:41:2&version=1.0";
  char *signstr = "app_id=2018102961967184&sign_type=RSA2";


  // case 1: sign and then verify
  ret = rsa_private_sign(privkeypath,signstr,&res,&sz_out) ;
  printf("\ndigest size: %zu, sign %sok!\n",strlen(signstr),ret>0?"":"NOT ");

  {
    unsigned char final_res[1024] = "";
    int sz_res = 1024;
    base64_encode(res,sz_out,final_res,&sz_res) ;
    printf("digest: %s\n",final_res);
  }

  ret = rsa_public_verify(pubkeypath,signstr,res,sz_out); 
  printf("result %smatches!\n",ret>0?"":"NOT ");

  if (res)
    OPENSSL_free(res);


#if 0
  // case 2: verify with given signature
  char signstr2[1024] = "";
  const char *__signstr = "gmt_create=2019-03-15 14:46:12&charset=UTF-8&seller_email=13192804289@163.com&subject=大同商城&body=大同商城&buyer_id=2088722583220144&invoice_amount=0.01&notify_id=2019031500222144614020140567317477&fund_bill_list=[{\"amount\":\"0.01\",\"fundChannel\":\"ALIPAYACCOUNT\"}]&notify_type=trade_status_sync&trade_status=TRADE_SUCCESS&receipt_amount=0.01&buyer_pay_amount=0.01&app_id=2019031163542035&sign_type=RSA2&seller_id=2088431720900668&gmt_payment=2019-03-15 14:46:13&notify_time=2019-03-15 14:46:14&passback_params=1&version=1.0&out_trade_no=1625363905-18147&total_amount=0.01&trade_no=2019031522001420140545069013&auth_app_id=2019031163542035&buyer_logon_id=159****2523&point_amount=0.00&";
  char *sign2 = "mzmTkQaxzelpIfNp5cwVV7yz0kJ4tOzwNJ9SNQOGMPCkUWNc4TTKZcVCwe/vWRTW2Uc2/DFQcdw699dr7mvEksGnX3/APdfslRpWwfjiDCrH6234cBH/Pa11E2OLHA9IdojNGwlyFpblRCojcJyqR8ZZADALsGRsFman6fA0k7MA1dz90e2HVtvyfLVdg3npvmtnI6NXdtoUOOSPKxhwPD2MVfzX71XATn2rGmc3JmFgEXEv3loix01PPQHpiFJwGsCs37MTQ2clHHO60AX/kuvfcjBVg7l4LybuBgW4tbAjJsv6CGG2GtspvppzlVfvuD7TNemAP/+A/Tw4ycJBtg==" ;
  const char pubkeypath2[] = "/home/user1/work/mnet/src/level5/pay_svr/keys/rsa_public_key.pem";
  unsigned char sign_dec[1024];
  int sz_dec = 1024 ;
  tree_map_t sign_map = new_tree_map();
  dbuffer_t signstr3 = 0;

  strcpy(signstr2,__signstr);
  uri_to_map(signstr2,strlen(signstr2),sign_map);
  drop_tree_map_item(sign_map,"sign_type",strlen("sign_type"));
  signstr3 = create_html_params(sign_map);
  printf("sign str: %s\n",signstr3);


  base64_decode((unsigned char*)sign2,strlen(sign2),sign_dec,&sz_dec);

  printf("dec size: %zu\n",(size_t)sz_dec);
  ret = rsa_public_verify(pubkeypath2,signstr3,sign_dec,(size_t)sz_dec); 
  printf("result 2 %smatches!\n",ret>0?"":"NOT ");

  delete_tree_map(sign_map);
#endif
}
#endif

