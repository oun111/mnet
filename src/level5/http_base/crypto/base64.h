#ifndef __BASE64_H__
#define __BASE64_H__

int base64_decode(unsigned char *src,int srclen,unsigned char *dst, int *dstlen);
int base64_encode(unsigned char *src,int srclen,unsigned char *dst,int *dstlen);

#endif
