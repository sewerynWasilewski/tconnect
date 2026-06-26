#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/rand.h>

#include "tconnect/transport.h"
#include "tconnect/url.h"

typedef struct {
  transport_t  base;
  transport_t *inner;
  char        *host;
  char        *port;
  char        *path;
  ws_opts_t    opts;
} ws_transport_t;

/* base64 encode src into dst - dst must be at least ((src_len+2)/3)*4+1 bytes */
static void b64_encode(const unsigned char *src, int src_len, char *dst) {
  static const char table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int i = 0, j = 0;
  unsigned char a3[3], a4[4];

  while (src_len--) {
    a3[i++] = *src++;
    if (i == 3) {
      a4[0] = (a3[0] & 0xfc) >> 2;
      a4[1] = ((a3[0] & 0x03) << 4) | ((a3[1] & 0xf0) >> 4);
      a4[2] = ((a3[1] & 0x0f) << 2) | ((a3[2] & 0xc0) >> 6);
      a4[3] = a3[2] & 0x3f;
      for (i = 0; i < 4; i++) dst[j++] = table[a4[i]];
      i = 0;
    }
  }
  if (i) {
    for (int k = i; k < 3; k++) a3[k] = 0;
    a4[0] = (a3[0] & 0xfc) >> 2;
    a4[1] = ((a3[0] & 0x03) << 4) | ((a3[1] & 0xf0) >> 4);
    a4[2] = ((a3[1] & 0x0f) << 2) | ((a3[2] & 0xc0) >> 6);
    for (int k = 0; k < i + 1; k++) dst[j++] = table[a4[k]];
    while (i++ < 3) dst[j++] = '=';
  }
  dst[j] = '\0';
}

static int ws_connect(transport_t *t, const char *host, const char *port) {
  ws_transport_t *ws = (ws_transport_t *)t;

  /* use stored host/port from ws_transport_from_url if caller passes NULL */
  const char *h = (host && *host) ? host : ws->host;
  const char *p = (port && *port) ? port : ws->port;

  if (!h || !p) {
    tconnect_set_error("ws_connect: no host/port — use transport_connect(t, host, port) or ws_transport_from_url");
    return TCONNECT_ERR_CONNECT;
  }

  if (transport_connect(ws->inner, h, p) != TCONNECT_OK) {
    return TCONNECT_ERR_CONNECT;
  }

  /* generate Sec-WebSocket-Key: 16 random bytes, base64 encoded */
  unsigned char rand_bytes[16];
  RAND_bytes(rand_bytes, sizeof(rand_bytes));
  char ws_key[25]; /* ceil(16/3)*4 + 1 = 25 */
  b64_encode(rand_bytes, sizeof(rand_bytes), ws_key);

  const char *path = ws->path ? ws->path : "/";

  char req[1024];
  int req_len = snprintf(req, sizeof(req),
    "GET %s HTTP/1.1\r\n"
    "Host: %s:%s\r\n"
    "Upgrade: websocket\r\n"
    "Connection: Upgrade\r\n"
    "Sec-WebSocket-Key: %s\r\n"
    "Sec-WebSocket-Version: 13\r\n"
    "\r\n",
    path, h, p, ws_key);

  transport_write(ws->inner, req, req_len);

  /* read response until \r\n\r\n - server keeps connection open after 101 */
  char buf[2048];
  size_t total = 0;
  while (total < sizeof(buf) - 1) {
    int n = transport_read(ws->inner, buf + total, sizeof(buf) - total - 1);
    if (n <= 0) break;
    total += n;
    buf[total] = '\0';
    if (strstr(buf, "\r\n\r\n")) break;
  }

  /* check for 101 Switching Protocols */
  if (!strstr(buf, "101")) {
    tconnect_set_error("WebSocket upgrade failed - no 101 in response");
    return TCONNECT_ERR_CONNECT;
  }

  return TCONNECT_OK;
}

/* byte 0: FIN=1 | opcode */
#define WS_FIN               0x80
#define WS_OPCODE_CONTINUE   0x00
#define WS_OPCODE_TEXT       0x01
#define WS_OPCODE_BINARY     0x02
#define WS_OPCODE_CLOSE      0x08
#define WS_OPCODE_PING       0x09
#define WS_OPCODE_PONG       0x0A
/* byte 1: MASK=1 | 7-bit length indicator */
#define WS_MASK              0x80
#define WS_LEN_16            126   /* next 2 bytes hold real length */
#define WS_LEN_64            127   /* next 8 bytes hold real length */

/* send a control frame (ping/pong/close) - always masked, always FIN=1, payload <= 125 */
static int ws_send_ctrl(ws_transport_t *ws, uint8_t opcode,
                        const void *payload, size_t len) {
  unsigned char header[2];
  header[0] = WS_FIN | opcode;
  header[1] = WS_MASK | (unsigned char)len;

  unsigned char mask[4];
  RAND_bytes(mask, sizeof(mask));

  unsigned char masked[125];
  for (size_t i = 0; i < len; i++)
    masked[i] = ((const unsigned char *)payload)[i] ^ mask[i % 4];

  transport_write(ws->inner, header, 2);
  transport_write(ws->inner, mask,   4);
  if (len > 0) transport_write(ws->inner, masked, len);
  return TCONNECT_OK;
}

int ws_read_frame(transport_t *t, void *buf, size_t buf_len, ws_frame_t *frame) {
  ws_transport_t *ws = (ws_transport_t *)t;
  size_t max = ws->opts.max_message_size ? ws->opts.max_message_size : WS_DEFAULT_MAX_MESSAGE_SIZE;

  /* read 2-byte header */
  unsigned char hdr[2];
  if (transport_read_exact(ws->inner, hdr, 2) != 0) return TCONNECT_ERR_CONNECT;

  bool     masked      = (hdr[1] & 0x80) != 0;
  uint64_t payload_len = (hdr[1] & 0x7F);

  /* extended length */
  if (payload_len == WS_LEN_16) {
    unsigned char ext[2];
    if (transport_read_exact(ws->inner, ext, 2) != 0) return TCONNECT_ERR_CONNECT;
    payload_len = ((uint64_t)ext[0] << 8) | ext[1];
  } else if (payload_len == WS_LEN_64) {
    unsigned char ext[8];
    if (transport_read_exact(ws->inner, ext, 8) != 0) return TCONNECT_ERR_CONNECT;
    payload_len = 0;
    for (int i = 0; i < 8; i++) payload_len = (payload_len << 8) | ext[i];
  }

  /* check declared length before any allocation */
  if (payload_len > max) {
    tconnect_set_error("ws frame payload %llu exceeds max_message_size (%zu)",
                       (unsigned long long)payload_len, max);
    return TCONNECT_ERR_ALLOC;
  }
  if (payload_len > buf_len) {
    tconnect_set_error("ws frame payload %llu exceeds caller buffer (%zu)",
                       (unsigned long long)payload_len, buf_len);
    return TCONNECT_ERR_ALLOC;
  }

  /* masking key (server→client frames should not be masked, but handle it) */
  unsigned char mask[4] = {0};
  if (masked) {
    if (transport_read_exact(ws->inner, mask, 4) != 0) return TCONNECT_ERR_CONNECT;
  }

  /* read payload */
  if (transport_read_exact(ws->inner, buf, (size_t)payload_len) != 0)
    return TCONNECT_ERR_CONNECT;

  if (masked) {
    for (size_t i = 0; i < (size_t)payload_len; i++)
      ((unsigned char *)buf)[i] ^= mask[i % 4];
  }

  frame->opcode      = hdr[0] & 0x0F;
  frame->fin         = (hdr[0] & 0x80) != 0;
  frame->payload     = (uint8_t *)buf;
  frame->payload_len = (size_t)payload_len;
  return TCONNECT_OK;
}

static int ws_read(transport_t *t, void *buf, size_t len) {
  if (!t) return -1;
  ws_transport_t *ws   = (ws_transport_t *)t;
  size_t          max  = ws->opts.max_message_size ? ws->opts.max_message_size
                                              : WS_DEFAULT_MAX_MESSAGE_SIZE;
  size_t total = 0;

  while (1) {
    if (total >= len) {
      tconnect_set_error("ws message exceeds caller buffer (%zu)", len);
      return TCONNECT_ERR_ALLOC;
    }

    ws_frame_t frame;
    int ret = ws_read_frame(t, (char *)buf + total, len - total, &frame);
    if (ret != TCONNECT_OK) return ret;

    if (frame.opcode == WS_OPCODE_PING) {
      ws_send_ctrl(ws, WS_OPCODE_PONG, frame.payload, frame.payload_len);
      continue;
    }

    if (frame.opcode == WS_OPCODE_CLOSE) {
      ws_send_ctrl(ws, WS_OPCODE_CLOSE, NULL, 0);
      tconnect_set_error("ws connection closed by server");
      return TCONNECT_ERR_CONNECT;
    }

    /* data frame - accumulate, guard against size_t overflow before adding */
    if (frame.payload_len > SIZE_MAX - total ||
        total + frame.payload_len > max) {
      tconnect_set_error("ws message exceeds max_message_size (%zu)", max);
      return TCONNECT_ERR_ALLOC;
    }

    total += frame.payload_len;
    if (frame.fin) return (int)total;
  }
}

static int ws_write(transport_t *t, const void *buf, size_t len) {
  if (!t) return -1;
  ws_transport_t *ws = (ws_transport_t *)t;

  /* build header - 2 bytes + optional 2 or 8 byte extended length */
  unsigned char header[10];
  int header_len = 0;

  header[0] = WS_FIN | WS_OPCODE_BINARY;

  if (len <= 125) {
    header[1] = WS_MASK | (unsigned char)len;
    header_len = 2;
  } else if (len <= 0xFFFF) {
    header[1] = WS_MASK | WS_LEN_16;
    header[2] = (len >> 8) & 0xFF;   /* big-endian */
    header[3] = (len     ) & 0xFF;
    header_len = 4;
  } else {
    header[1] = WS_MASK | WS_LEN_64;
    for (int i = 0; i < 8; i++)      /* big-endian 64-bit */
      header[2 + i] = (len >> (56 - 8 * i)) & 0xFF;
    header_len = 10;
  }

  /* generate mask key */
  unsigned char mask[4];
  RAND_bytes(mask, sizeof(mask));

  /* mask the payload */
  unsigned char *masked = malloc(len);
  if (!masked) return TCONNECT_ERR_ALLOC;
  for (size_t i = 0; i < len; i++)
    masked[i] = ((const unsigned char *)buf)[i] ^ mask[i % 4];

  /* send: header | mask key | masked payload */
  transport_write(ws->inner, header, header_len);
  transport_write(ws->inner, mask, sizeof(mask));
  int ret = transport_write(ws->inner, masked, len);

  free(masked);
  return ret;
}

static void ws_close(transport_t *t) {
  if (!t) return;
  ws_transport_t *ws = (ws_transport_t *)t;

  /* send close frame, then drain until server echoes close back or drops */
  ws_send_ctrl(ws, WS_OPCODE_CLOSE, NULL, 0);
  char drain[256];
  ws_frame_t frame;
  for (int i = 0; i < 10; i++) {
    if (ws_read_frame(t, drain, sizeof(drain), &frame) != TCONNECT_OK) break;
    if (frame.opcode == WS_OPCODE_CLOSE) break;
  }

  transport_close(ws->inner);
  free(ws->host);
  free(ws->port);
  free(ws->path);
}

static int ws_get_fd(transport_t *t) {
  if (!t) return -1;
  ws_transport_t *ws = (ws_transport_t *)t;
  return ws->inner->get_fd(ws->inner);
}

static transport_t *ws_alloc(transport_t *inner, const char *host, const char *port,
                             const char *path, ws_opts_t *opts) {
  ws_transport_t *ws = calloc(1, sizeof(ws_transport_t));
  if (!ws) return NULL;

  ws->inner        = inner;
  ws->host         = host ? strdup(host) : NULL;
  ws->port         = port ? strdup(port) : NULL;
  ws->path         = path ? strdup(path) : NULL;
  if (opts) ws->opts = *opts;
  ws->base.connect = ws_connect;
  ws->base.read    = ws_read;
  ws->base.write   = ws_write;
  ws->base.close   = ws_close;
  ws->base.get_fd  = ws_get_fd;

  return (transport_t *)ws;
}

transport_t *ws_transport_create(const char *path, ws_opts_t *opts) {
  return ws_alloc(tcp_transport_create(), NULL, NULL, path, opts);
}

transport_t *ws_transport_create_over(transport_t *inner, const char *path, ws_opts_t *opts) {
  return ws_alloc(inner, NULL, NULL, path, opts);
}

transport_t *ws_transport_from_url(const char *url_str, tls_opts_t *tls, ws_opts_t *ws_opts) {
  url_t *url = url_parse(url_str);
  if (!url) {
    tconnect_set_error("ws_transport_from_url: failed to parse URL");
    return NULL;
  }

  transport_t *inner;
  if (strcmp(url->protocol, "wss") == 0) {
    inner = tls_transport_create(tls);
  } else if (strcmp(url->protocol, "ws") == 0) {
    inner = tcp_transport_create();
  } else {
    tconnect_set_error("ws_transport_from_url: unsupported protocol '%s' (use ws:// or wss://)",
                       url->protocol);
    url_free(url);
    return NULL;
  }

  if (!inner) {
    url_free(url);
    return NULL;
  }

  transport_t *t = ws_alloc(inner, url->host, url->port, url->path, ws_opts);
  url_free(url);
  return t;
}
