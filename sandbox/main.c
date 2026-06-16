#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"
#include "tconnect/http.h"

/*
 * scenario: raw TLS connection to example.com:443
 *
 * no HTTP layer - just TLS transport sending a raw HTTP/1.1 request
 * and printing whatever comes back. proves TLS handshake works.
 */

int main(void) {
  tconnect_err_t err;
  http_response_t *resp = http_get("https://httpbin.org/get", NULL, NULL, &err);
  if (!resp) {
    fprintf(stderr, "request failed: %s\n", tconnect_last_error());
    return 1;
  }
  printf("status: %d %s\n", resp->status_code, resp->status_text);
  printf("body:\n%s\n", resp->body);
  http_response_free(resp);
  return 0;
}
