#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tconnect/transport.h"
#include "tconnect/async.h"
#include "tconnect/connection.h"

/*
 * scenario: three async connections using connection_t
 *   c1 (id=1, port 8081) - inline
 *   c2 (id=2, port 8082) - inline
 *   c3 (id=3, port 8083) - callback
 *
 * start three servers:
 *   terminal 1: nc -l 8081
 *   terminal 2: nc -l 8082
 *   terminal 3: nc -l 8083
 *
 * type in any terminal - messages appear here tagged with id.
 */

static void on_data(transport_t *t, void *userdata)
{
  (void)t;
  connection_t *c = (connection_t *)userdata;
  int n = connection_recv(c);
  if (n <= 0) {
    printf("[id=%d] connection closed (callback)\n", c->id);
    return;
  }
  printf("[id=%d][callback] %.*s", c->id, (int)c->buf_len, c->buf);
  connection_consume(c, c->buf_len);
}

static connection_t *connect_or_die(int id, const char *port)
{
  transport_t *t = tcp_transport_create();
  if (!t) { fprintf(stderr, "alloc failed\n"); return NULL; }

  printf("connecting id=%d to localhost:%s...\n", id, port);
  if (t->connect(t, "localhost", port) != 0) {
    fprintf(stderr, "connection to %s failed\n", port);
    return NULL;
  }

  connection_t *c = connection_create(t, id);
  printf("connected id=%d\n", id);
  return c;
}

/* find connection_t from a ready transport_t pointer */
static connection_t *find_conn(connection_t **conns, int n, transport_t *t)
{
  for (int i = 0; i < n; i++)
    if (conns[i]->transport == t) return conns[i];
  return NULL;
}

int main(void)
{
  connection_t *c1 = connect_or_die(1, "8081");
  connection_t *c2 = connect_or_die(2, "8082");
  connection_t *c3 = connect_or_die(3, "8083");
  if (!c1 || !c2 || !c3) return 1;

  connection_t *all[] = { c1, c2, c3 };

  tconnect_ctx_t *ctx = async_ctx_create();
  async_register(ctx, c1->transport, NULL,    NULL);  /* inline */
  async_register(ctx, c2->transport, NULL,    NULL);  /* inline */
  async_register(ctx, c3->transport, on_data, c3);    /* callback */

  printf("\nwaiting for messages (ctrl+c to quit)...\n\n");

  while (1) {
    transport_t *ready[8];
    int n = async_poll(ctx, ready, 8, -1);

    for (int i = 0; i < n; i++) {
      if (ready[i] == c3->transport) continue;  /* handled by callback */

      connection_t *c = find_conn(all, 3, ready[i]);
      if (!c) continue;

      int bytes = connection_recv(c);
      if (bytes <= 0) {
        printf("[id=%d] connection closed\n", c->id);
        async_unregister(ctx, c->transport);
        connection_free(c);
        continue;
      }
      printf("[id=%d][inline] %.*s", c->id, (int)c->buf_len, c->buf);
      connection_consume(c, c->buf_len);
    }
  }

  connection_free(c1);
  connection_free(c2);
  connection_free(c3);
  async_ctx_free(ctx);
  return 0;
}
