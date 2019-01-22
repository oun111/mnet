
#ifndef __CRYPTO_H__
#define __CRYPTO_H__

#ifdef __cplusplus
extern "C" {
#endif

extern int gen_auth_data(char*, size_t, const char*, unsigned char*);

extern unsigned long str2id(char*,size_t);

#ifdef __cplusplus
}
#endif

#endif /* __CRYPTO_H__*/
