
#ifndef __DBUFFER_H__
#define __DBUFFER_H__

/**
 * buffer definitions
 */

typedef char* dbuffer_t ;


extern dbuffer_t alloc_dbuffer(size_t sz);

extern dbuffer_t alloc_default_dbuffer();

extern dbuffer_t realloc_dbuffer(dbuffer_t b, size_t sz);

extern dbuffer_t rearrange_dbuffer(dbuffer_t b, size_t sz);

extern int is_dbuffer_valid(dbuffer_t b);

extern int drop_dbuffer(dbuffer_t b);

extern size_t dbuffer_data_size(dbuffer_t b);

extern size_t dbuffer_size(dbuffer_t b);

extern dbuffer_t write_dbuffer(dbuffer_t b, void *in, size_t inlen);

extern dbuffer_t append_dbuffer(dbuffer_t b, void *in, size_t inlen);

extern int read_dbuffer(dbuffer_t b, char *out, size_t *outlen);

extern off_t dbuffer_lseek(dbuffer_t b, off_t offset, int whence, int rw);

extern dbuffer_t dbuffer_ptr(dbuffer_t b, int rw);

extern dbuffer_t write_dbuffer_string(dbuffer_t b, char *in, size_t inlen);

extern dbuffer_t append_dbuffer_string(dbuffer_t b, char *in, size_t inlen);

extern int reset_dbuffer(dbuffer_t b);

#define write_dbuf_str(dbuf,s) ({ \
  dbuf = write_dbuffer_string(dbuf,(char*)s,strlen(s)); \
})

#define append_dbuf_str(dbuf,s) ({ \
  dbuf = append_dbuffer_string(dbuf,(char*)s,strlen(s)); \
})

#endif /* __DBUFFER_H__*/
