#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "formats.h"
#include "fmt0.h"
#include "log.h"


static
struct fmt0_info_s {
  regex_t r ;

  const char *ptn ; 

  dbuffer_t parse_buf ;
} 
g_fmtInfo = 
{
  // format is: [1 or 2 spaces]yyyy-MM-dd[1 space]hh:mm:ss
  .ptn = "[ ]{1,2}[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}",
} ;

static int init_fmt0();


static
int parse_record(common_format_t *cf, char *sp, size_t len)
{
  int yy=0,mm=0,dd=0,hh=0,MM=0,ss=0,ms=0,pid=0;
  const char *datetime_ptn = "yyyy-MM-dd hh:mm:ss";
  char *p = 0, *p0 = 0, ch = 0;
  cf_pair_t pc = 0;


  g_fmtInfo.parse_buf = write_dbuffer_string(g_fmtInfo.parse_buf,sp,len);

  p = g_fmtInfo.parse_buf ;

  while (isspace(*p)) p++;


  *cf = new_common_format();

  /* 
   * save date time column
   */
  pc = new_cf_pair();
  write_dbuf_str(pc->cf,"datetime");

  sscanf(p,"%d-%d-%d %d:%d:%d %d ",&yy,&mm,&dd,&hh,&MM,&ss,&ms);

  p0 = p + strlen(datetime_ptn) + 1;

  // the ms
  while (isdigit(*p0)) p0++ ;

  pc->val = write_dbuffer_string(pc->val,p,p0-p);

  insert_cf_pair2(*cf,pc);

  p = p0 + 1;

  log_debug("datetimes: %s,ms: %d\n",pc->val,ms);

  /* 
   * save pid column
   */
  pc = new_cf_pair();
  write_dbuf_str(pc->cf,"pid");

  sscanf(p,"(%d)",&pid);

  p = strchr(p,'(');
  p0= strchr(p+1,')');
  pc->val = write_dbuffer_string(pc->val,p+1,p0-p-1);

  insert_cf_pair2(*cf,pc);

  p = p0+1;

  log_debug("pids: %s\n",pc->val);

  /*
   * save log level column
   */
  pc = new_cf_pair();
  write_dbuf_str(pc->cf,"log_level");

  sscanf(p,"[%c]",&ch);
  
  p = strchr(p,'[');
  p0= strchr(p+1,']');
  pc->val = write_dbuffer_string(pc->val,p+1,p0-p-1);

  insert_cf_pair2(*cf,pc);

  p = p0+1;

  log_debug("log level: %s\n",pc->val);

  /* 
   * save log contents column
   */
  pc = new_cf_pair();
  write_dbuf_str(pc->cf,"contents");

  size_t sz = dbuffer_data_size(g_fmtInfo.parse_buf);
  p0 = dbuffer_ptr(g_fmtInfo.parse_buf,0) + sz;

  pc->val = write_dbuffer_string(pc->val,p,p0-p);

  insert_cf_pair2(*cf,pc);

  log_debug("contents: %s\n",pc->val);

  /*
   * save row no: hostname-pid-datetime
   */
  struct tm tm ;
  tm.tm_year= yy-1900;
  tm.tm_mon = mm-1;
  tm.tm_mday = dd;
  tm.tm_hour = hh;
  tm.tm_min = MM;
  tm.tm_sec = ss;
  time_t ts = (mktime(&tm)+2208988800)*1000000+ms;
  char rowno[96] = "";


  gethostname(rowno,sizeof(rowno));
  sprintf(rowno+strlen(rowno),"-%d-%ld",pid,ts);

  save_cf_row_no(*cf,rowno);

  log_debug("row-no: %s\n",(*cf)->row);

  
  //log_debug("new record: %s\n",g_fmtInfo.parse_buf);
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
  int begin = 0, next = 0, end = 0;
  int rc = 0;


  *endp = '\0';

  for (char *ps=0,*pe=0;;) {

    int ret = regexec(&g_fmtInfo.r,rp+next,nmatch,pmatch,0);
    if (ret==REG_NOMATCH) {
      // ps=0 means cant recognize format, otherwise, means
      //  a record begin is found but no ending 
      rc = ps==NULL?-1:0;
      //log_error("format not recognize!\n");
      break ;
    }

    begin= pmatch[0].rm_so ;
    end  = pmatch[0].rm_eo ;

    // record begin
    if (!ps) {
      ps = rp + next + begin ;
      next += end ;
    } 
    // record ending
    else if (ps)  {
      pe = rp + next + begin ;

      parse_record(cf,ps,pe-ps);

      dbuffer_lseek(inb,end,SEEK_CUR,0);

      rc = 1;
      break ;
    }
  }

  *endp = ch ;
  
  // may try next line...
  return rc ;
}

static
void release_fmt0()
{
  regfree(&g_fmtInfo.r);

  drop_dbuffer(g_fmtInfo.parse_buf);
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

  g_fmtInfo.parse_buf = alloc_default_dbuffer();

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
    " 2019-10-12 00:03:59 3481 (962) [D] default_proto_close: invoked\n"
    "  2019-10-14 12:51:30 220823 (962) [D] do_alipay_order: user out trade no '100392692-015670' done!\n"
    "  2019-10-14 12:51:33 75056 (962) [D] http_svr_rx: POST /alipay/pay HTTP/1.1\n"
    "Content-Type: text/xml\n"
"Content-Length: 234\n"
"Host: 39.108.78.104:5481\n"
"Connection: Keep-Alive\n"
"User-Agent: Apache-HttpClient/4.5.1 (Java/1.7.0_191)\n"
"Accept-Encoding: gzip,deflate\n"
"\n"
"body=货款&mch_id=mch_001&notify_url=http://119.23.23.172:4230/epl_datong/huitongAlih5payNotifyResponse&out_trade_no=100392635-015678&return_url=http://119.23.23.172:4230/bmall/app/cash/progress&subject=大同商城&total_amount=15.0...(size 443) - ok\n"
    " 2019-10-14 12:51:30 138125 (962) [D] myredis_read: begin to sync, try read later\n";
  dbuffer_t dbuf = alloc_default_dbuffer();
  common_format_t cf = 0;


  write_dbuf_str(dbuf,inb);
  
  while (parse(dbuf,&cf)==1) ;

}

