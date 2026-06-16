#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"
#include "tconnect/http.h"

#define HTTP_BUF_SIZE 65536

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
      login_s = p;        login_l    = colon - p;
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
    host_s = p;        host_l = colon - p;
    port_s = colon + 1; port_l = host_end - colon - 1;
  } else {
    host_s = p; host_l = host_end - p;
    int https = (protocol_l == 5 && strncmp(protocol_s, "https", 5) == 0);
    port_s = https ? "443" : "80"; // TO DO default port registry
    port_l = https ? 3 : 2;
  }

  /* path?query */
  if (slash) {
    const char *qmark = strchr(slash, '?');
    if (qmark) {
      path_s  = slash;    path_l  = qmark - slash;
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

/* returns pointer into resp->headers_raw - valid while resp is alive, do not free.
 * null-terminates the value in-place on first call for each header. */
const char *http_header_get(http_response_t *resp, const char *name) {
  if (!resp || !resp->headers_raw || !name) return NULL;

  char *p = resp->headers_raw;
  size_t name_len = strlen(name);

  while (*p) {
    if (strncasecmp(p, name, name_len) == 0 && p[name_len] == ':') {
      char *val = p + name_len + 1;
      while (*val == ' ') val++;   /* skip leading space */
      char *eol = strstr(val, "\r\n");
      if (eol) *eol = '\0';        /* null-terminate in-place */
      return val;
    }
    char *eol = strstr(p, "\r\n");
    if (!eol) break;
    p = eol + 2;
  }
  return NULL;
}

void http_response_print(const http_response_t *resp) {
  if (!resp) { printf("(null response)\n"); return; }
  printf("HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text ? resp->status_text : "");
  if (resp->headers_raw) printf("%s\r\n", resp->headers_raw);
  printf("\r\n");
  if (resp->body && resp->body_len > 0)
    printf("%.*s\n", (int)resp->body_len, resp->body);
}

void http_response_free(http_response_t *resp){
  if (!resp) return;
  free(resp->status_text);
  free(resp->headers_raw);
  free(resp->body);
  free(resp);
}

/* parse raw bytes into http_response_t */
static http_response_t *parse_response(const char *raw, size_t len) {
  http_response_t *resp = calloc(1, sizeof(http_response_t));
  if (!resp) return NULL;

  /* status line: "HTTP/1.1 200 OK\r\n" */
  const char *p = raw;
  const char *line_end = strstr(p, "\r\n");
  if (!line_end) { http_response_free(resp); return NULL; }

  /* skip "HTTP/x.x " */
  const char *code_start = strchr(p, ' ');
  if (!code_start) { http_response_free(resp); return NULL; }
  code_start++;

  resp->status_code = atoi(code_start);

  const char *text_start = strchr(code_start, ' ');
  if (text_start)
    resp->status_text = strndup(text_start + 1, line_end - text_start - 1);

  /* headers block: from after status line to \r\n\r\n */
  const char *headers_start = line_end + 2;
  const char *headers_end   = strstr(headers_start, "\r\n\r\n");
  if (!headers_end) { http_response_free(resp); return NULL; }

  resp->headers_raw = strndup(headers_start, headers_end - headers_start);

  /* body: everything after \r\n\r\n */
  const char *body_start = headers_end + 4;
  resp->body_len = len - (body_start - raw);
  if (resp->body_len > 0) {
    resp->body = malloc(resp->body_len + 1);
    memcpy(resp->body, body_start, resp->body_len);
    resp->body[resp->body_len] = '\0';
  }

  return resp;
}

#define SET_ERR(err_ptr, code) do { if (err_ptr) *(err_ptr) = (code); } while(0)

// TODO: add http_request_t for logging/middleware
static http_response_t *do_request(const char *method, const char *url_str,
                                   const char *body, const char **extra_headers,
                                   http_opts_t *opts, tconnect_err_t *err) {
  SET_ERR(err, TCONNECT_OK);

  const char *version = (opts && opts->http_version) ? opts->http_version : "1.1";

  if (strcmp(version, "1.0") != 0 && strcmp(version, "1.1") != 0) {
    tconnect_set_error("unsupported HTTP version '%s' (supported: 1.0, 1.1)", version);
    SET_ERR(err, TCONNECT_ERR_UNSUPPORTED);
    return NULL;
  }
  int              number_of_redirects = 0;
  int              max_redirects = (opts && opts->follow_redirects) ? opts->max_redirects : 0;
  http_response_t *resp         = NULL;
  char            *redirect_url = NULL;  /* heap-owned, NULL = using caller's url_str */

  while (number_of_redirects <= max_redirects) {
    url_t *url = url_parse(url_str);
    if (!url) {
      tconnect_set_error("url_parse failed: allocation error or malformed URL");
      free(redirect_url);
      SET_ERR(err, TCONNECT_ERR_ALLOC);
      return NULL;
    }

    transport_t *t; 
    if(strcmp(url->protocol, "https") == 0) { 
      t = tls_transport_create(opts ? opts->tls : NULL); 
    } else { 
      t = tcp_transport_create();
    }

    if (!t) {
      tconnect_set_error("transport allocation failed");
      url_free(url);
      free(redirect_url);
      SET_ERR(err, TCONNECT_ERR_ALLOC);
      return NULL;
    }

    if (transport_connect(t, url->host, url->port) != 0) {
      transport_close(t);
      url_free(url);
      free(redirect_url);
      SET_ERR(err, TCONNECT_ERR_CONNECT);
      return NULL;
    }

    /* build request line + mandatory headers */
    char req[4096];
    int  req_len = snprintf(req, sizeof(req),
      "%s %s%s%s HTTP/%s\r\n"
      "Host: %s\r\n"
      "Connection: close\r\n",
      method,
      url->path,
      url->query ? "?" : "",
      url->query ? url->query : "",
      version,
      url->host);

    /* append extra headers if provided */
    if (extra_headers) {
      for (int i = 0; extra_headers[i]; i++) {
        req_len += snprintf(req + req_len, sizeof(req) - req_len,
                            "%s\r\n", extra_headers[i]);
      }
    }

    /* body */
    if (body) {
      req_len += snprintf(req + req_len, sizeof(req) - req_len,
                          "Content-Length: %zu\r\n\r\n%s", strlen(body), body);
    } else {
      req_len += snprintf(req + req_len, sizeof(req) - req_len, "\r\n");
    }

    transport_write(t, req, req_len);

    /* read response into buffer */
    char  *buf   = malloc(HTTP_BUF_SIZE);
    size_t total = 0;
    int    n;

    while ((n = transport_read(t, buf + total, HTTP_BUF_SIZE - total - 1)) > 0)
      total += n;
    buf[total] = '\0';

    transport_close(t);
    url_free(url);

    http_response_free(resp);
    resp = parse_response(buf, total);
    free(buf);

    if (!resp) {
      tconnect_set_error("failed to parse HTTP response");
      free(redirect_url);
      SET_ERR(err, TCONNECT_ERR_PARSE);
      return NULL;
    }

    if (!(300 <= resp->status_code && resp->status_code < 400))
      break;

    /* redirect - extract Location and copy it before resp is freed next iteration */
    const char *location = http_header_get(resp, "Location");
    if (!location) break;

    /* build new URL before freeing redirect_url - url_str may point into it */
    char *new_url = strdup(location);  /* own a copy - location points into resp which we free next iteration */

    /* handle relative redirect: /path → http://host:port/path */
    if (new_url[0] == '/') {
      url_t *base = url_parse(url_str);  /* url_str still valid here */
      char  *full = malloc(strlen(base->protocol) + strlen(base->host) + strlen(base->port) + strlen(new_url) + 10);
      sprintf(full, "%s://%s:%s%s", base->protocol, base->host, base->port, new_url);
      url_free(base);
      free(new_url);
      new_url = full;
    }

    free(redirect_url);   /* safe to free now - url_str no longer needed */
    redirect_url = new_url;
    url_str      = redirect_url;
    ++number_of_redirects;
  }

  free(redirect_url);
  return resp;
}

http_response_t *http_get(const char *url, const char **headers,
                          http_opts_t *opts, tconnect_err_t *err) {
  return do_request("GET", url, NULL, headers, opts, err);
}

http_response_t *http_post(const char *url, const char *body,
                           const char **headers, http_opts_t *opts,
                           tconnect_err_t *err) {
  return do_request("POST", url, body, headers, opts, err);
}
