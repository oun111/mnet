#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "file_cache.h"
#include "log.h"


int load_file(const char *path, dbuffer_t *inb)
{
  const size_t read_size = 128 ;
  char fbuf[read_size];
  int fd = open(path,O_RDONLY);
  ssize_t sz = 0;

  if (fd<0) {
    log_error("open file %s fail\n",path);
    return -1;
  }

  if (!*inb)
    *inb = alloc_default_dbuffer();

  while ((sz=read(fd,fbuf,read_size))>0) 
    *inb = append_dbuffer(*inb,fbuf,sz);

  close(fd);
  return 0;
}
