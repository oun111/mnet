#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include "formats.h"
#include "fmt0.h"
#include "log.h"


static
struct fmt0_info_s {
  regex_t r ;

  const char *ptn ; 
} 
g_fmtInfo = 
{
  // format is: [1 or 2 spaces]yyyy-MM-dd[1 space]hh:mm:ss
  .ptn = "[ ]{1,2}[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}",
} ;

static int init_fmt0();


static
int fmt0_detail_proc(common_format_t *cf, dbuffer_t inb, int recbegin, int recend)
{
  char recbuf[256] = "";

  strncpy(recbuf,inb+recbegin,recend-recbegin);
  recbuf[recend-recbegin] = '\0';
  
  log_debug("new record: %s\n",recbuf);
  return 0;
}

static
int parse(dbuffer_t inb, common_format_t *cf)
{
  size_t sz= dbuffer_data_size(inb);
  char *rp = dbuffer_ptr(inb,0), *endp = rp+sz;
  const char ch = *endp;
  const int nmatch = 1;
  regmatch_t pmatch[nmatch];
  int ret = 0;
  int recbegin = 0, matchnext = 0, recend = 0;


  //log_debug("contents: %s\n",rp);
  *endp = '\0';

  // find head
  ret = regexec(&g_fmtInfo.r,rp,nmatch,pmatch,0);

  if (ret==REG_NOMATCH) {
    log_error("format not recognize!\n");
    //dbuffer_lseek(inb,sz,SEEK_CUR,0);
    *endp = ch ;
    return -1;
  }

  recbegin = pmatch[0].rm_so ;
  matchnext = pmatch[0].rm_eo ;


  // find ending
  ret = regexec(&g_fmtInfo.r,rp+matchnext,nmatch,pmatch,0);

  if (ret==REG_NOMATCH) {
    log_info("no record ending found now...\n");
    *endp = ch ;
    return 0;
  }

  recend = pmatch[0].rm_so - 1 ;

  fmt0_detail_proc(cf,rp,recbegin,recend);

  dbuffer_lseek(inb,recend,SEEK_CUR,0);
  
  // may try next line...
  return 1;
}

static
void release_fmt0()
{
  regfree(&g_fmtInfo.r);
}

static
struct formats_cb_s fmt0_cb = 
{
  .fmt_id  = 100,

  .init    = init_fmt0,
  .release = release_fmt0,
  .parser  = parse,
} ;

static
int init_fmt0()
{
  regcomp(&g_fmtInfo.r,g_fmtInfo.ptn,REG_EXTENDED);

  log_info("format with id '%d' init OK!\n",fmt0_cb.fmt_id);

  return 0;
}

int register_fmt0(formats_entry_t entry)
{
  add_format(entry,&fmt0_cb);

  return 0;
}

void parse_test()
{
  char *inb = 
    "  2019-10-12 00:03:59 3474 (962) [D] do_close: closing client 11\n"
    " 2019-10-12 00:03:59 3481 (962) [D] default_proto_close: invoked\n";
  dbuffer_t dbuf = alloc_default_dbuffer();

  write_dbuf_str(dbuf,inb);


  parse(dbuf,0);
}

