#ifndef TCONNECT_HTTP_H
#define TCONNECT_HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include "transport.h"

/* parsed URL - all fields are heap-allocated, free with url_free() */
typedef struct {
  char *_buf;      /* single allocation - all fields point into this */
  char *protocol;
  char *login;
  char *password;
  char *host;
  char *port;
  char *path;
  char *query;
} url_t;

typedef struct {
  const char *http_version;  /* "1.0" or "1.1" */
  bool follow_redirects;
  int max_redirects;  
} http_opts_t;

typedef struct {
  int    status_code;   /* 200, 404, etc */
  char  *status_text;   /* "OK", "Not Found", etc */
  char  *headers_raw;   /* raw header block after status line */
  char  *body;          /* response body */
  size_t body_len;
} http_response_t;

url_t          *url_parse(const char *url);
void            url_free(url_t *url);

http_response_t *http_get (const char *url, const char **headers, http_opts_t *opts, tconnect_err_t *err);
http_response_t *http_post(const char *url, const char *body, const char **headers, http_opts_t *opts, tconnect_err_t *err);
void             http_response_free(http_response_t *resp);

/* returned pointer is valid while resp is alive - do not free it.
 * null-terminates the value in-place on first call for each header. */
const char *http_header_get(http_response_t *resp, const char *name);

#endif
