#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"

/*
 * WebSocket sandbox - connects to the local test server.
 *
 * start the server first:
 *   source scripts/.venv/bin/activate
 *   python3 scripts/ws_test_server.py
 *
 * then run this binary.
 */

int main(void) {
  printf("connecting to ws://localhost:8081/...\n");

  transport_t *t = ws_transport_create("/", NULL);
  if (!t) { fprintf(stderr, "alloc failed\n"); return 1; }

  if (transport_connect(t, "localhost", "8081") != TCONNECT_OK) {
    fprintf(stderr, "connect failed: %s\n", tconnect_last_error());
    transport_close(t); free(t); return 1;
  }
  printf("WebSocket handshake complete\n\n");

  /* send a message */
  const char *msg = "hello from tconnect";
  transport_write(t, msg, strlen(msg));
  printf("sent: %s\n", msg);

  /* read one response */
  char buf[4096];
  int n = transport_read(t, buf, sizeof(buf) - 1);
  if (n > 0) {
    buf[n] = '\0';
    printf("received (%d bytes): %s\n", n, buf);
  } else {
    fprintf(stderr, "read failed: %s\n", tconnect_last_error());
  }

  transport_close(t);
  free(t);
  return 0;
}
