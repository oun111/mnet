#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include "dbuffer.h"


int url_encode(char *inb, size_t sz_in, dbuffer_t *outb)
{
  const char hex[] = "0123456789ABCDEF";
  
  
  write_dbuf_str(*outb,"");

  for (size_t i=0;i<sz_in;i++) {
    unsigned char c = inb[i] ;

    if (c==' ') {
      append_dbuf_str(*outb,"+");
    }
    else if ((c < '0' && c != '-' && c != '.') ||
      (c < 'A' && c > '9') ||
      (c > 'Z' && c < 'a' && c != '_') ||
      (c > 'z')) 
    {
      append_dbuf_str(*outb,"%");
      *outb = append_dbuffer_string(*outb,(char*)&hex[c>>4],1);
      *outb = append_dbuffer_string(*outb,(char*)&hex[c&15],1);
    }
    else {
      *outb = append_dbuffer_string(*outb,inb+i,1);
    }
  }

  return 0;
}

int url_decode(char *inb, size_t sz_in, dbuffer_t *outb)
{
#define tonum(c) ((c)>='0'&&(c)<='9'?(c)-'0':(c)-'a'+10)

  for (size_t i=0;i<sz_in;i++) {
    char c = inb[i], v = 0 ;

    if (c=='+') {
      v = ' ';
    }
    else if (c=='%' && (i+2)<sz_in && isxdigit(inb[i+1]) && 
             isxdigit(inb[i+2])) 
    {
      v = tonum(tolower(inb[i+1]))*16 + tonum(tolower(inb[i+2])) ;
      i += 2;
    }
    else {
      v = c ;
    }
    *outb = append_dbuffer_string(*outb,&v,1);
  }
  return 0;
}

#if TEST_CASES==1
void test_url_coder()
{
  char inb[] = "http://cache.baiducontent.com/c?m=9d78d513d98401f14fede52951478a304e40c72362d88a533996d25f9217465c0223a6ac27554158839b213240b8492bbbad696f6d5837b7ed9cdf1688a6d035258a2431721a805612a443e99418&p=c43ed516d9c11fe840bd9b7d0c1482&newp=8b2a9718879d1ee818bd9b7d0c1483231610db2151d7da146b82c825d7331b001c3bbfb423261703d7c2766c07a94e5beff63c713c0927a3dda5c91d9fb4c57479d5766a2f5bc6&user=baidu&fm=sc&query=url+encode&qid=fe18257800010e3d&p1=9";
  //char inb1[] = "https://www.baidu.com/s?ie=utf-8&f=3&rsv_bp=1&tn=95752409_hao_pg&wd=c%20url%20encode&oq=url%2520encode&rsv_pq=c780576400005bf7&rsv_t=4a96cptJkzdiG%2FaMijtW%2FZoXo1O0DD5gqpleoJ0nRQ4OZvPFST6q5IQfkaEJeNtgJ0SAlrSC&rqlang=cn&rsv_enter=1&rsv_sug3=4&rsv_sug1=1&rsv_sug7=100&rsv_sug2=0&prefixsug=c%2520url%2520encode&rsp=2&inputT=848&rsv_sug4=1768";
  dbuffer_t outb = alloc_default_dbuffer();
  dbuffer_t outb1 = alloc_default_dbuffer();

  url_encode(inb,strlen(inb),&outb);
  printf("input: %s\nresult: %s\n",inb,outb);

  url_decode(outb,strlen(outb),&outb1);
  if (!memcmp(inb,outb1,strlen(inb))) {
    printf("decode ok!\n");
  } else {
    printf("decode error! result: %s\n",outb1);
  }

  drop_dbuffer(outb);
  drop_dbuffer(outb1);
}
#endif
