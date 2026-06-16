#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"

/*
 * scenario: raw TLS connection to example.com:443
 *
 * no HTTP layer - just TLS transport sending a raw HTTP/1.1 request
 * and printing whatever comes back. proves TLS handshake works.
 */

int main(void) {
  transport_t *t = tls_transport_create(NULL);
  if (!t) { fprintf(stderr, "alloc failed\n"); return 1; }

  printf("connecting to httpbin.org:443 via TLS...\n");
  if (t->connect(t, "httpbin.org", "443") != TCONNECT_OK) {
    fprintf(stderr, "TLS connect failed: %s\n", tconnect_last_error());
    return 1;
  }
  printf("TLS handshake complete\n\n");

  const char *request =
    "GET / HTTP/1.1\r\n"
    "Host: httpbin.org\r\n"
    "Connection: close\r\n"
    "\r\n";

  transport_write(t, request, strlen(request));

  char buf[8192] = {0};
  size_t total = 0;
  int n;
  while ((n = transport_read(t, buf + total, sizeof(buf) - total - 1)) > 0)
    total += n;

  printf("received %zu bytes:\n\n%s\n", total, buf);

  t->close(t);
  return 0;
}
