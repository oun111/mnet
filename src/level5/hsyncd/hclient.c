
#include "hclient.h"
#include "log.h"


int hclient_init(hclient_t cln, const char *host, const int cport)
{
  int port = !(cport>0&&cport<65535)?9090:cport ;

  cln->socket = g_object_new(THRIFT_TYPE_SOCKET,"hostname",host,
                             "port",port,NULL);


  cln->transport = g_object_new(THRIFT_TYPE_BUFFERED_TRANSPORT,
                                "transport",cln->socket,NULL);

  thrift_transport_open(cln->transport, NULL);
  if (!thrift_transport_is_open(cln->transport)) {
    log_error("open socket fail!\n");
    return -1;
  }

  cln->protocol = g_object_new(THRIFT_TYPE_BINARY_PROTOCOL,
                               "transport",cln->transport,NULL);

  cln->client = g_object_new(TYPE_T_H_BASE_SERVICE_CLIENT,
                             "input_protocol",cln->protocol,
                             "output_protocol",cln->protocol,
                             NULL);
  return 0;
}

void hclient_release(hclient_t cln)
{
  if (G_IS_OBJECT(cln->client)) {
    g_object_unref(cln->client);
  }
  if (G_IS_OBJECT(cln->protocol)) {
    g_object_unref(cln->protocol);
  }
  if (G_IS_OBJECT(cln->transport)) {
    g_object_unref(cln->transport);
  }
  if (G_IS_OBJECT(cln->socket)) {
    g_object_unref(cln->socket);
  }
  if (G_IS_OBJECT(cln->client)) {
    g_object_unref(cln->client);
  }
}

int hclient_exists(hclient_t cln, const char *tablename)
{
  GByteArray *table = NULL ;
  TGet *tget = NULL ;
  gboolean ret = FALSE;
  TIOError *io = NULL;
  GError *error = NULL;


  log_debug("sending 'exists' command for table '%s'*******\n",tablename);

  tget = g_object_new(TYPE_T_GET,NULL);

  tget->row = g_byte_array_new();
  g_byte_array_append(tget->row,(guint8*)"0",strlen("0"));

  table= g_byte_array_new();
  g_byte_array_append(table,(guint8*)tablename,strlen(tablename));


  ret = t_h_base_service_client_exists(cln->client,&ret,table,tget,&io,&error);  
  if (ret==FALSE) {
    log_error("client exists fail(%d): %s (%s)\n",
           error->code,error->message,io->message);
  }
  else {
    log_info("success!\n");
  }

  g_byte_array_unref(table);
  g_object_unref(tget);
  g_object_unref(io);

  return ret==TRUE?0:-1;
}

int hclient_get(hclient_t cln, const char *tablename, const char *row, 
                const char *cf, char *val)
{
  GByteArray *table = NULL ;
  TGet *tget = NULL ;
  gboolean ret = FALSE;
  TIOError *io = NULL;
  GError *error = NULL;
  TResult *result= NULL;


  log_debug("sending 'get' command for table '%s'*******\n",tablename);

  // construct TGet
  tget = g_object_new(TYPE_T_GET,NULL);
  tget->row = g_byte_array_new();
  g_byte_array_append(tget->row,(guint8*)row,strlen(row));

  // construct TColumn
  if (cf && strlen(cf)>0) {
    TColumn *column = g_object_new(TYPE_T_COLUMN,NULL);
    column->family = g_byte_array_new();
    g_byte_array_append(column->family,(guint8*)cf,strlen(cf));
    tget->__isset_columns = TRUE;
    g_ptr_array_add(tget->columns,column);
  }

  // construct table
  table= g_byte_array_new();
  g_byte_array_append(table,(guint8*)tablename,strlen(tablename));

  // result
  result = g_object_new(TYPE_T_RESULT,NULL);

  ret = t_h_base_service_client_get(cln->client,&result,table,tget,&io,&error);  
  if (ret==FALSE) {
    log_error("client get fail(%d): %s (%s)\n",error?error->code:-1,
           error?error->message:"n/A",io?io->message:"N/a");
  }
  else {
    log_debug("row: %s\n",result->__isset_row?(gchar*)result->row->data:"none");

    for (int i=0;i<result->columnValues->len;i++) {
      TColumnValue *pcol = result->columnValues->pdata[i];
      log_debug("col family '%s'[%d], value '%s'[%d], qualifier '%s'\n",
             (gchar*)pcol->family->data,pcol->family->len,
             (gchar*)pcol->value->data,pcol->value->len,
             (gchar*)pcol->qualifier->data);

      if (val) {
        strncpy(val,(gpointer)pcol->value->data,pcol->value->len);
        val[pcol->value->len] = '\0';
      }
    }

    log_info("success!\n");
  }

  g_byte_array_unref(table);
  g_object_unref(tget);
  g_object_unref(result);
  if (G_IS_OBJECT(io))
    g_object_unref(io);

  return ret==TRUE?0:-1;
}

int hclient_put(hclient_t cln, const char *tablename, 
                const char *row, const char *cf, const char *val)
{
  GByteArray *table = NULL ;
  TPut *tput = NULL ;
  TColumnValue *cval = NULL;
  gboolean ret = FALSE;
  TIOError *io = NULL;
  GError *error = NULL;


  //log_debug("sending 'put' command for table '%s'*******\n",tablename);

  // construct TPut
  tput = g_object_new(TYPE_T_PUT,NULL);
  tput->row = g_byte_array_new();
  g_byte_array_append(tput->row,(guint8*)row,strlen(row));

  // columns
  cval = g_object_new(TYPE_T_COLUMN_VALUE,NULL);
  // family
  cval->family = g_byte_array_new();
  g_byte_array_append(cval->family,(guint8*)cf,strlen(cf));
  // value
  cval->value = g_byte_array_new();
  g_byte_array_append(cval->value,(guint8*)val,strlen(val));
  // qualifier
  cval->qualifier = NULL;
  g_ptr_array_add(tput->columnValues,(gpointer)cval);

  // construct table
  table= g_byte_array_new();
  g_byte_array_append(table,(guint8*)tablename,strlen(tablename));

  ret = t_h_base_service_client_put(cln->client,table,tput,&io,&error);  
  if (ret==FALSE) {
    log_error("client put fail(%d): %s (%s)\n",error?error->code:-1,
           error?error->message:"n/A",io?io->message:"N/a");
  }
  else {
    //log_info("success!\n");
  }

  g_byte_array_unref(table);
  g_object_unref(tput);
  if (G_IS_OBJECT(io))
    g_object_unref(io);
  if (G_IS_OBJECT(cval))
    g_object_unref(cval);

  return ret==TRUE?0:-1;
}

int hclient_delete(hclient_t cln, const char *tablename, 
                   const char *row, const char *cf)
{
  GByteArray *table = NULL ;
  TDelete *tdel = NULL ;
  TColumn *cval = NULL;
  gboolean ret = FALSE;
  TIOError *io = NULL;
  GError *error = NULL;


  log_debug("sending 'delete' command for table '%s'*******\n",tablename);

  // construct TDelete
  tdel = g_object_new(TYPE_T_DELETE,NULL);
  tdel->row = g_byte_array_new();
  g_byte_array_append(tdel->row,(guint8*)row,strlen(row));

  // columns
  if (cf && strlen(cf)>0) {
    cval = g_object_new(TYPE_T_COLUMN,NULL);
    // family
    cval->family = g_byte_array_new();
    g_byte_array_append(cval->family,(guint8*)cf,strlen(cf));
    tdel->__isset_columns = TRUE;
    g_ptr_array_add(tdel->columns,(gpointer)cval);
  }

  // construct table
  table= g_byte_array_new();
  g_byte_array_append(table,(guint8*)tablename,strlen(tablename));

  ret = t_h_base_service_client_delete_single(cln->client,table,tdel,&io,&error);  
  if (ret==FALSE) {
    log_error("client delete fail(%d): %s (%s)\n",error?error->code:-1,
           error?error->message:"n/A",io?io->message:"N/a");
  }
  else {
    log_debug("success!\n");
  }

  g_byte_array_unref(table);
  g_object_unref(tdel);
  if (G_IS_OBJECT(io))
    g_object_unref(io);

  return ret==TRUE?0:-1;
}
