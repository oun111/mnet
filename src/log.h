
#ifndef __LOG_H__
#define __LOG_H__

#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>
#include "dbuffer.h"


struct log_s {
  FILE *fd_info ;
  FILE *fd_debug ;
  FILE *fd_error ;
  char name[32];
  char path[PATH_MAX];
  dbuffer_t msg_buf ;

  struct date_t {
    int year;
    int month;
    int day;
  } curr;

  int pid;
} ;

typedef struct log_s* log_t ;

#define DECLARE_LOG \
  struct log_s g_log = { \
    .fd_info  = 0, \
    .fd_debug = 0, \
    .fd_error = 0, \
  };

extern struct log_s g_log ;


extern 
int init_log(log_t log, char *path, char *name);

extern
int close_log(log_t log);

extern
ssize_t write_log(log_t log,char *buf,const size_t sz_buf,const int log_type);

extern
void __hex_dump(FILE *fd, char *buf, size_t len);

extern
int do_flush_log(FILE *fd);

extern
void update_log_pid(log_t log);


enum log_type {
  l_dbg,
  l_err,
  l_inf,
} ;

#define log_info(fmt,arg...) do{\
  char buf[2048]="";\
  snprintf(buf, 2048, "[I] %s: " fmt ,\
    __func__, ## arg ); \
  write_log(&g_log,buf,sizeof(buf),l_inf);\
} while(0)

#define log_debug(fmt,arg...) do{\
  char buf[2048]="";\
  snprintf(buf, 2048,"[D] %s: " fmt ,\
    __func__, ## arg ); \
  write_log(&g_log,buf,sizeof(buf),l_dbg);\
} while(0)

#define log_error(fmt,arg...) do{\
  char buf[2048]="";\
  snprintf(buf, 2048,"[E] %s: " fmt ,\
    __func__, ## arg ); \
  write_log(&g_log,buf,sizeof(buf),l_err);\
} while(0)

#define hex_dump(buf,ln) do{\
  __hex_dump(g_log.fd_debug,buf,ln);  \
} while(0)

#define flush_logs() do{\
  do_flush_log(g_log.fd_info);  \
  do_flush_log(g_log.fd_error);  \
  do_flush_log(g_log.fd_debug);  \
} while(0)

#define save_log_pid() update_log_pid(&g_log)

#endif /* __LOG_H__*/

