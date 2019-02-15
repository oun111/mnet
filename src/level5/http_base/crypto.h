#ifndef __CRYPTO_H__
#define __CRYPTO_H__


extern int rsa_private_sign(const char *priv_key_path, const char *plain_text, 
                            unsigned char **res, unsigned int *sz_out);

extern int rsa_public_verify(const char *pub_key_path, const char *plain_text,
                             unsigned char *res, unsigned int sz_out);


#endif /* __CRYPTO_H__*/
