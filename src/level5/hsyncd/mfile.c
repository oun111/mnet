#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include "mfile.h"
#include "kernel.h"
#include "mm_porting.h"
#include "myrbtree.h"
#include "log.h"


static int compare(char *s0, char *s1)
{
  return strcmp(s0,s1) ;
}

mfile_t get_mfile(mfile_entry_t entry, const char *f)
{
  mfile_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,(char*)f,p,filename,node,compare)) 
    return p ;

  return NULL ;
}

dbuffer_t get_mfile_contents(mfile_t p)
{
  return p->cont_buf ;
}

static
mfile_t create_empty_mfile(mfile_entry_t entry, const char *f)
{
  mfile_t p = 0;

  if (!MY_RB_TREE_FIND(&entry->u.root,(char*)f,p,filename,node,compare)) {
    log_debug("monitor file '%s' exists\n",f);
    return p ;
  }

  //p = kmalloc(sizeof(struct monitor_file_s),0L);
  p = obj_pool_alloc(entry->pool,struct monitor_file_s);
  if (!p) {
    p = obj_pool_alloc_slow(entry->pool,struct monitor_file_s);
  }

  if (!p)
    return NULL ;

  p->file_offset= 0L; 
  write_dbuf_str(p->filename,f) ;
  //p->cont_size  = 0L; 
  //p->rp = 0L;

  if (MY_RB_TREE_INSERT(&entry->u.root,p,filename,node,compare)) {
    log_error("insert mfile '%s' fail\n",f);
    //kfree(p);
    obj_pool_free(entry->pool,p);
    return NULL;
  }

  log_info("new mfile '%s'\n",f);

  entry->count++;

  return p;
}

static
int do_load_file(const char *f, size_t startpos, size_t len, 
                 char *buf, int *size)
{
  int ret = 0;
  int fd = -1;

  fd = open(f,O_RDONLY);
  if (fd<0) {
    log_error("open '%s' fail: %s\n",f,strerror(errno));
    return -1;
  }

  ret = pread(fd,buf,len,startpos);
  if (ret>0) {
    *size = ret ;
  }

  close(fd);

  return 0;
}

static
int __mfile_load(mfile_entry_t entry, const char *f, mfile_t p, int size)
{
  char *wb = 0;
  int sz = 0,ret = 0,sz_read=0;


  if (unlikely(size<p->file_offset)) {
    log_error("read fail, new size: %d, read pos: %zu\n",
              size,p->file_offset);
    return -1;
  }

  sz_read = size - p->file_offset;
  if (sz_read<entry->sync_threshold) {
    log_info("no need to read file now\n");
    return 0;
  }

  // prepare buffer
  p->cont_buf = rearrange_dbuffer(p->cont_buf,sz_read);
  wb = dbuffer_ptr(p->cont_buf,1);

  ret = do_load_file(f,p->file_offset,sz_read,wb,&sz);
  if (ret<0) {
    log_debug("read '%s' fail!\n",f);
    return -1;
  }

  //p->cont_size += sz ;
  p->file_offset += sz ;

  log_debug("loaded %d bytes\n",sz);

  dbuffer_lseek(p->cont_buf,sz,SEEK_CUR,1);
  return 1;
}

bool is_mfile_sync(mfile_entry_t entry, mfile_t p)
{
  return dbuffer_data_size(p->cont_buf) >= entry->sync_threshold ;
}

int 
load_mfile(mfile_entry_t entry, const char *f, size_t size, mfile_t *pf)
{
  mfile_t p = create_empty_mfile(entry,f);


  if (!p) {
    log_error("can't create mfile '%s'\n", f);
    return -1;
  }

  if (pf)
    *pf = p ;

  // try to read file
  return __mfile_load(entry,f,p,size);
}

int rename_mfile(mfile_entry_t entry, const char *oldfile, const char *newfile)
{
  mfile_t p = get_mfile(entry,oldfile);


  if (!p) {
    log_error("mfile '%s' not found\n",oldfile);
    return -1;
  }

  // remove old file entry from rb tree
  rb_erase(&p->node,&entry->u.root);

  write_dbuf_str(p->filename,newfile) ;

  // reinsert to tree with new file
  if (MY_RB_TREE_INSERT(&entry->u.root,p,filename,node,compare)) {
    log_error("insert mfile '%s' fail\n",newfile);
    obj_pool_free(entry->pool,p);
    return -1;
  }

  return 0;
}

static
int drop_mfile_internal(mfile_entry_t entry, mfile_t p)
{
  rb_erase(&p->node,&entry->u.root);

  obj_pool_free(entry->pool,p);

  entry->count --;

  return 0;
}

int drop_mfile(mfile_entry_t entry, const char *f)
{
  mfile_t p = get_mfile(entry,f);

  if (!p) {
    return -1;
  }

  drop_mfile_internal(entry,p);

  return 0;
}

int init_mfile_entry(mfile_entry_t entry, size_t threshold, ssize_t pool_size)
{
  mfile_t pos,n;


  entry->u.root = RB_ROOT ;
  entry->count  = 0L;

  entry->pool = create_obj_pool("mfile-pool",pool_size,struct monitor_file_s);

  list_for_each_objPool_item(pos,n,entry->pool) {
    pos->filename = alloc_default_dbuffer();
    pos->cont_buf = alloc_default_dbuffer();
  }

  entry->sync_threshold = threshold>10240?4096:SIZE_ALIGNED(threshold) ;

  log_debug("sync threshold: %zu\n",entry->sync_threshold);

  log_debug("done!\n");

  return 0;
}

int release_all_mfiles(mfile_entry_t entry)
{
  mfile_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    drop_mfile_internal(entry,pos);
  }

  list_for_each_objPool_item(pos,n,entry->pool) {
    drop_dbuffer(pos->filename);
    drop_dbuffer(pos->cont_buf);
  }

  release_obj_pool(entry->pool,struct monitor_file_s);

  return 0;
}


size_t get_mfile_count(mfile_entry_t entry)
{
  return entry->count ;
}

void iterate_mfiles(mfile_entry_t entry)
{
  mfile_t pos,n;

  rbtree_postorder_for_each_entry_safe(pos,n,&entry->u.root,node) {
    log_debug("mfile '%s', file pos: %ld\n",pos->filename,pos->file_offset);
  }
}

