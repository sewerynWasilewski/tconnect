#include <stdio.h>
#include <stdlib.h>
#include "tconnect/transport.h"
#include "tconnect/http.h"

/*
 * HTTP GET and POST using the HTTP layer.
 */

int main(void) {
  tconnect_err_t err;

  printf("=== GET /get ===\n");
  http_response_t *r1 = http_get("http://httpbin.org/get", NULL, NULL, &err);
  if (!r1) { fprintf(stderr, "GET failed: %s\n", tconnect_last_error()); return 1; }
  http_response_print(r1);
  http_response_free(r1);

  printf("=== POST /post ===\n");
  const char *headers[] = { "Content-Type: application/json", NULL };
  http_response_t *r2 = http_post(
    "http://httpbin.org/post",
    "{\"message\": \"hello from tconnect\"}",
    headers, NULL, &err);
  if (!r2) { fprintf(stderr, "POST failed: %s\n", tconnect_last_error()); return 1; }
  http_response_print(r2);
  http_response_free(r2);

  printf("=== GET /redirect/2 (follow) ===\n");
  http_opts_t opts = { .follow_redirects = true, .max_redirects = 5 };
  http_response_t *r3 = http_get("http://httpbin.org/redirect/2", NULL, &opts, &err);
  if (!r3) { fprintf(stderr, "redirect failed: %s\n", tconnect_last_error()); return 1; }
  http_response_print(r3);
  http_response_free(r3);

  return 0;
}
