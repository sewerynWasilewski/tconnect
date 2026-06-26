#ifndef TCONNECT_URL_H
#define TCONNECT_URL_H

/* parsed URL — all string fields point into a single internal buffer.
 * free the whole thing with url_free(). */
typedef struct {
  char *_buf;
  char *protocol;
  char *login;
  char *password;
  char *host;
  char *port;
  char *path;
  char *query;
} url_t;

url_t *url_parse(const char *raw);
void   url_free(url_t *url);

#endif
