#include <stdio.h>
#include <stdlib.h>
#include "tconnect/transport.h"
#include "tconnect/http.h"

/*
 * HTTPS GET using the HTTP layer — TLS is handled automatically.
 */

int main(void) {
  tconnect_err_t err;
  http_response_t *resp = http_get("https://httpbin.org/get", NULL, NULL, &err);
  if (!resp) {
    fprintf(stderr, "request failed: %s\n", tconnect_last_error());
    return 1;
  }

  http_response_print(resp);

  http_response_free(resp);
  return 0;
}
