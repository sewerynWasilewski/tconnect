#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"

/*
 * WSS sandbox - connects to the local TLS test server.
 *
 * generate cert (once):
 *   openssl req -x509 -newkey rsa:2048 -keyout scripts/server.key \
 *     -out scripts/server.crt -days 365 -nodes -subj "/CN=localhost"
 *
 * start the server:
 *   source scripts/.venv/bin/activate
 *   python3 scripts/ws_test_server.py --tls
 *
 * then run this binary.
 */

int main(void) {
  printf("connecting to wss://localhost:8081/...\n");

  tls_opts_t tls = {
    .verify_peer = true,
    .ca_file     = "scripts/server.crt",
  };
  transport_t *t = ws_transport_from_url("wss://localhost:8081/", &tls, NULL);
  if (!t) { fprintf(stderr, "alloc failed\n"); return 1; }

  if (transport_connect(t, NULL, NULL) != TCONNECT_OK) {
    fprintf(stderr, "connect failed: %s\n", tconnect_last_error());
    transport_close(t); free(t); return 1;
  }
  printf("WSS handshake complete\n\n");

  const char *msg = "hello from tconnect over wss";
  transport_write(t, msg, strlen(msg));
  printf("sent: %s\n", msg);

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
