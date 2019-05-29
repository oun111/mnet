#ifndef __CRYPTO_H__
#define __CRYPTO_H__


struct rsa_entry_s {
  void *sign_item ;

  void *versign_item ;
} ;
typedef struct rsa_entry_s* rsa_entry_t ;


extern int rsa_private_sign(const char *priv_key_path, const char *plain_text, 
                            unsigned char **res, size_t *sz_out);

extern int rsa_public_verify(const char *pub_key_path, const char *plain_text,
                             unsigned char *res, size_t sz_out);

extern int rsa_private_sign1(rsa_entry_t entry, const char *plain_text, 
                             unsigned char **res, size_t *sz_out);

extern int rsa_public_verify1(rsa_entry_t entry, const char *plain_text,
                              unsigned char *res, size_t sz_out);

extern int init_rsa_entry(rsa_entry_t entry, const char *pub_key_path, 
                          const char *priv_key_path);

extern int release_rsa_entry(rsa_entry_t entry);

#endif /* __CRYPTO_H__*/
