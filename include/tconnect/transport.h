#ifndef TCONNECT_TRANSPORT_H
#define TCONNECT_TRANSPORT_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>

typedef struct transport_t transport_t;

typedef enum {
  TCONNECT_OK              =   0,
  TCONNECT_ERR_ALLOC       = -10,
  TCONNECT_ERR_CONNECT     = -11,
  TCONNECT_ERR_UNSUPPORTED = -12,
  TCONNECT_ERR_PARSE       = -13,
  TCONNECT_TLS_INIT_ERR    = -14,
} tconnect_err_t;

/* last error string — thread-local, set internally on failure */
static _Thread_local char _tconnect_last_error[256];

static inline void tconnect_set_error(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(_tconnect_last_error, sizeof(_tconnect_last_error), fmt, ap);
  va_end(ap);
}

static inline const char *tconnect_last_error(void)
{
  return _tconnect_last_error[0] ? _tconnect_last_error : "no error";
}

static inline const char *tconnect_strerror(int err) {
  if (err > -10 && err < 0) return "system error — check errno";
  switch ((tconnect_err_t)err) {
    case TCONNECT_OK:              return "ok";
    case TCONNECT_ERR_ALLOC:       return "allocation failed";
    case TCONNECT_ERR_CONNECT:     return "connection failed";
    case TCONNECT_ERR_UNSUPPORTED: return "unsupported";
    case TCONNECT_ERR_PARSE:       return "parse error";
    case TCONNECT_TLS_INIT_ERR:    return "TLS init failed";
  }
  return "unknown error";
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
