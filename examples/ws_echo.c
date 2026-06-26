#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"

/*
 * ws_echo — plain WebSocket echo over ws://
 *
 * start a local echo server first:
 *   source scripts/.venv/bin/activate
 *   python3 scripts/ws_test_server.py
 */

int main(void) {
  transport_t *t = ws_transport_from_url("ws://localhost:8081/", NULL, NULL);
  if (!t) { fprintf(stderr, "alloc failed\n"); return 1; }

  if (transport_connect(t, NULL, NULL) != TCONNECT_OK) {
    fprintf(stderr, "connect failed: %s\n", tconnect_last_error());
    transport_close(t); free(t); return 1;
  }
  printf("connected to ws://localhost:8081/\n");

  const char *msg = "hello over ws";
  transport_write(t, msg, strlen(msg));
  printf("sent:     %s\n", msg);

  char buf[4096];
  int n = transport_read(t, buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    printf("received: %s\n", buf);
  } else {
    fprintf(stderr, "read failed: %s\n", tconnect_last_error());
  }

  transport_close(t);
  free(t);
  return 0;
}
