#ifndef TCONNECT_CONNECTION_H
#define TCONNECT_CONNECTION_H

#include "transport.h"
#include <stddef.h>

#define TCONNECT_BUF_SIZE 4096

typedef struct {
  transport_t *transport;
  int          id;
  char         buf[TCONNECT_BUF_SIZE];
  size_t       buf_len;
} connection_t;

static inline connection_t *connection_create(transport_t *t, int id)
{
  connection_t *c = calloc(1, sizeof(connection_t));
  if (!c) return NULL;
  c->transport = t;
  c->id        = id;
  return c;
}

static inline void connection_free(connection_t *c)
{
  if (!c) return;
  c->transport->close(c->transport);
  free(c);
}

/* appends new bytes to the accumulation buffer, returns bytes read or -1 */
static inline int connection_recv(connection_t *c)
{
  size_t space = TCONNECT_BUF_SIZE - c->buf_len;
  if (space == 0) return -1;
  int n = transport_read(c->transport, c->buf + c->buf_len, space);
  if (n > 0) c->buf_len += n;
  return n;
}

/* consume n bytes from the front of the buffer */
static inline void connection_consume(connection_t *c, size_t n)
{
  if (n >= c->buf_len) {
    c->buf_len = 0;
    return;
  }
  memmove(c->buf, c->buf + n, c->buf_len - n);
  c->buf_len -= n;
}

#endif
