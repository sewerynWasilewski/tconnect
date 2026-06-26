#ifndef TCONNECT_TRANSPORT_H
#define TCONNECT_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
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

/* last error string - thread-local, set internally on failure.
 * defined once in src/url.c — extern so all translation units share one instance */
extern _Thread_local char _tconnect_last_error[256];

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
  if (err > -10 && err < 0) return "system error - check errno";
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

/* reads exactly len bytes, looping on short reads. returns 0 on success,
 * negative on error, -1 if the connection closed before len bytes arrived. */
static inline int transport_read_exact(transport_t *t, void *buf, size_t len) {
  size_t total = 0;
  while (total < len) {
    int n = transport_read(t, (char *)buf + total, len - total);
    if (n < 0) return n;
    if (n == 0) {
      tconnect_set_error("connection closed after %zu of %zu bytes", total, len);
      return -1;
    }
    total += (size_t)n;
  }
  return 0;
}


typedef struct {
  int recv_buf_size;  /* 0 = use OS default */
  int send_buf_size;
} transport_opts_t;

transport_t *tcp_transport_create(void);
void         tcp_transport_free(transport_t *t);
transport_t *tcp_transport_create_opts(transport_opts_t *opts);

transport_t *udp_transport_create(void);
void         udp_transport_free(transport_t *t);
transport_t *udp_transport_create_opts(transport_opts_t *opts);


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

#define WS_DEFAULT_MAX_MESSAGE_SIZE (16 * 1024 * 1024)  /* 16 MB */

typedef struct {
  size_t max_message_size;  /* 0 = use default (16MB) */
} ws_opts_t;

/* frame metadata — payload points into the caller-provided buffer, do not free */
typedef struct {
  uint8_t  opcode;
  bool     fin;
  uint8_t *payload;
  size_t   payload_len;
} ws_frame_t;

/* ws_transport_create: path is the URL path e.g. "/chat", NULL = "/".
 * ws_transport_create_over: caller provides inner transport (pass TLS for wss://) */
transport_t *ws_transport_create(const char *path, ws_opts_t *opts);
transport_t *ws_transport_create_over(transport_t *inner, const char *path, ws_opts_t *opts);
/* parses ws:// or wss:// URL — selects TCP or TLS inner transport automatically.
 * call transport_connect(t, NULL, NULL) to connect using the URL's host/port. */
transport_t *ws_transport_from_url(const char *url, tls_opts_t *tls, ws_opts_t *opts);

/* reads one raw frame into buf. returns 0 on success, negative on error.
 * control frames (ping/pong/close) are returned as-is — caller must handle them. */
int ws_read_frame(transport_t *t, void *buf, size_t buf_len, ws_frame_t *frame);

#endif
