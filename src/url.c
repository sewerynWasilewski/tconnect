#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"
#include "tconnect/url.h"

/* single definition of the thread-local error buffer — all TUs share this via extern */
_Thread_local char _tconnect_last_error[256];

static const char *default_port(const char *proto, size_t len) {
  if ((len == 5 && strncmp(proto, "https", 5) == 0) ||
      (len == 3 && strncmp(proto, "wss", 3) == 0))
    return "443";
  return "80";
}

url_t *url_parse(const char *raw) {
  const char *p = raw;

  const char *protocol_s; size_t protocol_l;
  const char *login_s    = NULL; size_t login_l    = 0;
  const char *password_s = NULL; size_t password_l = 0;
  const char *host_s;     size_t host_l;
  const char *port_s;     size_t port_l;
  const char *path_s;     size_t path_l;
  const char *query_s    = NULL; size_t query_l    = 0;

  /* protocol */
  const char *proto_end = strstr(p, "://");
  if (proto_end) {
    protocol_s = p; protocol_l = proto_end - p;
    p = proto_end + 3;
  } else {
    protocol_s = "http"; protocol_l = 4;
  }

  /* login:password@ */
  const char *at    = strchr(p, '@');
  const char *slash = strchr(p, '/');
  if (at && (!slash || at < slash)) {
    const char *colon = memchr(p, ':', at - p);
    if (colon) {
      login_s    = p;        login_l    = colon - p;
      password_s = colon + 1; password_l = at - colon - 1;
    } else {
      login_s = p; login_l = at - p;
    }
    p = at + 1;
  }

  /* host:port */
  slash = strchr(p, '/');
  const char *host_end = slash ? slash : p + strlen(p);
  const char *colon    = memchr(p, ':', host_end - p);
  if (colon) {
    host_s = p;         host_l = colon - p;
    port_s = colon + 1; port_l = host_end - colon - 1;
  } else {
    host_s = p; host_l = host_end - p;
    port_s = default_port(protocol_s, protocol_l);
    port_l = strlen(port_s);
  }

  /* path?query */
  if (slash) {
    const char *qmark = strchr(slash, '?');
    if (qmark) {
      path_s  = slash;     path_l  = qmark - slash;
      query_s = qmark + 1; query_l = strlen(qmark + 1);
    } else {
      path_s = slash; path_l = strlen(slash);
    }
  } else {
    path_s = "/"; path_l = 1;
  }

  size_t total = protocol_l+1 + host_l+1 + port_l+1 + path_l+1
               + (login_l    ? login_l+1    : 0)
               + (password_l ? password_l+1 : 0)
               + (query_l    ? query_l+1    : 0);

  url_t *url = calloc(1, sizeof(url_t));
  if (!url) return NULL;

  url->_buf = malloc(total);
  if (!url->_buf) { free(url); return NULL; }

#define PACK(field, src, len) \
  do { url->field = b; memcpy(b, src, len); b[len] = '\0'; b += len + 1; } while(0)

  char *b = url->_buf;
  PACK(protocol, protocol_s, protocol_l);
  PACK(host,     host_s,     host_l);
  PACK(port,     port_s,     port_l);
  PACK(path,     path_s,     path_l);
  if (login_l)    { PACK(login,    login_s,    login_l); }
  if (password_l) { PACK(password, password_s, password_l); }
  if (query_l)    { PACK(query,    query_s,    query_l); }

#undef PACK

  return url;
}

void url_free(url_t *url) {
  if (!url) return;
  free(url->_buf);
  free(url);
}
