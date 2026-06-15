#include <stdio.h>
#include <stdlib.h>
#include "tconnect/transport.h"
#include "tconnect/http.h"

/*
 * scenario: HTTP requests using the http layer
 *
 * tests:
 *   1. GET  localhost:8089/get        - basic GET, inspect response
 *   2. POST localhost:8089/post       - POST with JSON body
 *   3. GET  localhost:8089/status/404 - non-200 status code handling
 */

static void print_response(const char *label, http_response_t *resp) {
  if (!resp) {
    printf("[%s] request failed\n\n", label);
    return;
  }

  printf("[%s] raw response:\n", label);
  printf("HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text ? resp->status_text : "");
  if (resp->headers_raw)
    printf("%s\r\n", resp->headers_raw);
  printf("\r\n");
  if (resp->body && resp->body_len > 0)
    printf("%.*s", (int)resp->body_len, resp->body);
  printf("\n\n");
}

int main(void) {
  /* 1. basic GET */
  printf("=== GET /get ===\n");
  tconnect_err_t err;
  http_response_t *r1 = http_get("http://localhost:8089/get", NULL, NULL, &err);
  if (!r1) { fprintf(stderr, "GET failed: %s\n", tconnect_strerror(err)); return 1; }
  print_response("GET", r1);
  http_response_free(r1);

  /* 2. POST with JSON body and Content-Type header */
  printf("=== POST /post ===\n");
  const char *headers[] = {
    "Content-Type: application/json",
    NULL
  };
  http_response_t *r2 = http_post(
    "http://localhost:8089/post",
    "{\"message\": \"hello from tconnect\"}",
    headers,
    NULL,
    &err
  );
  if (!r2) { fprintf(stderr, "POST failed: %s\n", tconnect_strerror(err)); return 1; }
  print_response("POST", r2);
  http_response_free(r2);

  /* 3. non-200 response */
  printf("=== GET /status/404 ===\n");
  http_response_t *r3 = http_get("http://localhost:8089/status/404", NULL, NULL, &err);
  print_response("404", r3);
  http_response_free(r3);

  /* 4. redirect - follow 2 hops, expect final 200 */
  printf("=== GET /redirect/2 (follow_redirects=true, max=5) ===\n");
  http_opts_t redirect_opts = {
    .http_version     = "1.0",
    .follow_redirects = true,
    .max_redirects    = 5,
  };
  http_response_t *r4 = http_get("http://localhost:8089/redirect/2", NULL, &redirect_opts, &err);
  if (!r4) { fprintf(stderr, "redirect GET failed: %s\n", tconnect_strerror(err)); return 1; }
  printf("final status: %d %s\n\n", r4->status_code, r4->status_text ? r4->status_text : "");
  http_response_free(r4);

  /* 5. redirect - disabled, expect 302 */
  printf("=== GET /redirect/2 (follow_redirects=false) ===\n");
  http_opts_t no_redirect_opts = {
    .http_version     = "1.1",
    .follow_redirects = false,
  };
  http_response_t *r5 = http_get("http://localhost:8089/redirect/2", NULL, &no_redirect_opts, &err);
  if (!r5) { fprintf(stderr, "GET failed: %s\n", tconnect_strerror(err)); return 1; }
  printf("status (expect 302): %d\n", r5->status_code);
  printf("Location: %s\n\n", http_header_get(r5, "Location") ? http_header_get(r5, "Location") : "(none)");
  http_response_free(r5);

  return 0;
}
