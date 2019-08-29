
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <alloca.h>
#include "log.h"



/*
 * \e[F;Bm F 字體顏色 30-37 B 背景顏色 40-47 
 *
 * F B 
 * 30 40 黑 
 * 31 41 紅 
 * 32 42 綠 
 * 33 43 黃 
 * 34 44 藍 
 * 35 45 紫紅 
 * 36 46 靛藍 
 * 37 47 白 
 *
 */

#define COLOR_INF  "\e[34;48m"
#define COLOR_DBG  "\e[33;48m"
#define COLOR_ERR  "\e[31;48m"
#define COLOR_END  "\e[0m"


static 
int do_close_log(log_t log, bool bRename)
{
  if (log->fd) {
    fflush(log->fd);
    fclose(log->fd);
  }

  if (bRename==true) {
    char logpath[PATH_MAX] = "", tmp[PATH_MAX] = "";
    char fmt[] = "%s/%s.log.%d-%d-%d",
         oldfmt[] = "%s/%s.log";

    sprintf(tmp,oldfmt,log->path,log->name);
    sprintf(logpath,fmt,log->path,log->name,
        log->curr.year+1900,log->curr.month+1,log->curr.day);
    rename(tmp,logpath);
  }

  return 0;
}

static
int do_open_log(log_t log)
{
  char logpath[PATH_MAX] = "";
  char fmt[] = "%s/%s.log";

  sprintf(logpath,fmt,log->path,log->name);

  log->fd = fopen(logpath,"a");

  return 0;
}

int new_log(log_t log, bool renameOld)
{
  struct tm tm1 ;
  time_t t = time(0);

  do_close_log(log, renameOld);

  do_open_log(log);

  localtime_r(&t,&tm1);

  log->curr.year = tm1.tm_year;
  log->curr.month= tm1.tm_mon;
  log->curr.day  = tm1.tm_mday;

  return 0;
}

int init_log(log_t log, char *path, char *name)
{
  strcpy(log->path,path);
  strcpy(log->name,name);
  log->fd = NULL;

  printf("%s: logger name '%s', logging to %s\n",__func__,name,path);

  return new_log(log,false);
}

int close_log(log_t log)
{
  do_close_log(log,false);

  if (is_dbuffer_valid(log->msg_buf))
    drop_dbuffer(log->msg_buf);

  return 0;
}

ssize_t 
write_log(log_t log, char *msg,const size_t sz_buf, const int log_type)
{
  struct timeval tv ;
  struct tm tm1 ;
  int ret = 0;
  char *mb = 0;
  char *color_end = "", *color = "" ;
  FILE *fd = log->fd;


  gettimeofday(&tv,NULL);
  localtime_r(&tv.tv_sec,&tm1);

  // check if date changes
  if (!(tm1.tm_year==log->curr.year && tm1.tm_mon==log->curr.month && 
      tm1.tm_mday==log->curr.day)) {
    new_log(log,true);
  }

  if (!fd) {
    fd    = stdout;
    color = log_type==l_inf?COLOR_INF:
            log_type==l_dbg?COLOR_DBG:
            COLOR_ERR ;
    color_end = COLOR_END ;
  }


  // get the message buffer
  if (!is_dbuffer_valid(log->msg_buf)) {
    log->msg_buf = alloc_dbuffer(sz_buf+100);
  } 
  else  log->msg_buf = realloc_dbuffer(log->msg_buf,sz_buf+100);

  mb = log->msg_buf ;


  snprintf(mb,sz_buf+100,"%s %d-%02d-%02d %02d:%02d:%02d %ld (%d) %s %s",
           color,
           tm1.tm_year+1900,tm1.tm_mon+1,tm1.tm_mday,tm1.tm_hour,tm1.tm_min,
           tm1.tm_sec,tv.tv_usec,
           log->pid,
           msg,
           color_end);

  ret = fwrite(mb,strlen(mb),1,fd);
  if (ret<=0)
    return -1;

  return 0;
}

void __hex_dump(FILE *fd, char *buf, size_t len)
{
  size_t i=0;
  int cnt=0;

  if (!fd)
    fd = stdout;

  for (;i<len;i++) {
    fprintf(fd, "%02x ", buf[i]&0xff);
    if (cnt++>14) {
      cnt=0;
      fprintf(fd,"\n");
    }   
  }
  fprintf(fd,"\n");
}

int do_flush_log(FILE *fd)
{
  if (!fd)
    fd = stdout;

  fflush(fd);

  return 0;
}

void update_log_pid(log_t log)
{
  log->pid = getpid();
}

int flush_log(void *unuse, void *ptimeouts)
{
  flush_logs();
  //log_info("pid %d flushing logs...\n",getpid());

  return 0;
}

