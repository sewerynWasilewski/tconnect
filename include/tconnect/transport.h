#ifndef TCONNECT_TRANSPORT_H
#define TCONNECT_TRANSPORT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct transport_t transport_t;

typedef enum {
  TCONNECT_OK              = 0,
  TCONNECT_ERR_ALLOC       = 1,
  TCONNECT_ERR_CONNECT     = 2,
  TCONNECT_ERR_UNSUPPORTED = 3,
  TCONNECT_ERR_PARSE       = 4,
} tconnect_err_t;

static inline const char *tconnect_strerror(tconnect_err_t err) {
  switch(err){ 
    case TCONNECT_OK:              return "OK"; 
    case TCONNECT_ERR_ALLOC:       return "Allocation Error"; 
    case TCONNECT_ERR_CONNECT:     return "Connection Error";
    case TCONNECT_ERR_UNSUPPORTED: return "Unsuported Error";
    case TCONNECT_ERR_PARSE:       return "Parse Error";
  }
  return "Unknown Error Code"; 
}

struct transport_t {
  int  (*connect)(transport_t *t, const char *host, char const *port);
  int  (*read)   (transport_t *t, void *buf, size_t len);
  int  (*write)  (transport_t *t, const void *buf, size_t len);
  void (*close)  (transport_t *t);

  /* returns the underlying fd for use with epoll/select.
   * return -1 if this transport has no fd (event loop will skip it) */
  int (*get_fd)(transport_t *t);
};
static inline int transport_connect (transport_t *t, const char *host, char const *port) { return t->connect(t, host, port); }
static inline int transport_read (transport_t *t, void *buf, size_t len)                 { return t->read(t, buf, len); }
static inline int transport_write(transport_t *t, const void *buf, size_t len)           { return t->write(t, buf, len); }
static inline void transport_close(transport_t *t)                                       { t->close(t); }


transport_t *tcp_transport_create(void);
void         tcp_transport_free(transport_t *t);

typedef struct {
  int recv_buf_size;  /* 0 = use OS default */
  int send_buf_size;
} transport_opts_t;

transport_t *tcp_transport_create_opts(transport_opts_t *opts);

typedef struct {
  int         min_version;   /* 0 = library default (TLS 1.2) */
  int         max_version;   /* 0 = no limit */
  bool        verify_peer;   /* true = verify server cert (default) */
  const char *ca_file;       /* custom CA cert file, NULL = system store */
  const char *client_cert;   /* mutual TLS client certificate file */
  const char *client_key;    /* mutual TLS private key file */
  const char *sni_hostname;  /* override SNI, NULL = use host from connect() */
} tls_opts_t;

transport_t *tls_transport_create(tls_opts_t *opts);
transport_t *tls_transport_create_over(transport_t *inner, tls_opts_t *opts);

#endif
