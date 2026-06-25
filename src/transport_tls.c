#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/x509_vfy.h>

#include "tconnect/transport.h"

typedef struct {
  transport_t  base;
  transport_t *inner;
  SSL_CTX     *ctx;
  SSL         *ssl;
  tls_opts_t   opts;
} tls_transport_t;

/* custom BIO - OpenSSL reads/writes through inner transport_t, not fd directly */
static int bio_write(BIO *b, const char *buf, int len) {
  transport_t *inner = BIO_get_data(b);
  return transport_write(inner, buf, len);
}

static int bio_read(BIO *b, char *buf, int len) {
  transport_t *inner = BIO_get_data(b);
  return transport_read(inner, buf, len);
}

static long bio_ctrl(BIO *b, int cmd, long num, void *ptr) {
  (void)b; (void)num; (void)ptr;
  if (cmd == BIO_CTRL_FLUSH) return 1;
  return 0;
}

static BIO *make_transport_bio(transport_t *inner) {
  BIO_METHOD *method = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "tconnect");
  if (!method) return NULL;
  BIO_meth_set_write(method, bio_write);
  BIO_meth_set_read (method, bio_read);
  BIO_meth_set_ctrl (method, bio_ctrl);

  BIO *bio = BIO_new(method);
  if (!bio) { BIO_meth_free(method); return NULL; }
  BIO_set_data(bio, inner);
  BIO_set_init(bio, 1);
  return bio;
}

static SSL_CTX *create_ctx(tls_opts_t *opts) {
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) return NULL;

  int min_ver = (opts && opts->min_version) ? opts->min_version : TLS1_2_VERSION;
  int max_ver = (opts && opts->max_version) ? opts->max_version : 0;
  SSL_CTX_set_min_proto_version(ctx, min_ver);
  if (max_ver) SSL_CTX_set_max_proto_version(ctx, max_ver);

  int verify = !opts || opts->verify_peer;
  SSL_CTX_set_verify(ctx, verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);

  if (verify) {
    if (opts && opts->ca_file)
      SSL_CTX_load_verify_locations(ctx, opts->ca_file, NULL);
    else
      SSL_CTX_set_default_verify_paths(ctx);
  }

  if (opts && opts->client_cert)
    SSL_CTX_use_certificate_file(ctx, opts->client_cert, SSL_FILETYPE_PEM);
  if (opts && opts->client_key)
    SSL_CTX_use_PrivateKey_file(ctx, opts->client_key, SSL_FILETYPE_PEM);

  return ctx;
}

static int tls_connect(transport_t *t, const char *host, const char *port) {
  tls_transport_t *tr = (tls_transport_t *)t;

  if (transport_connect(tr->inner, host, port) != 0)
    return TCONNECT_ERR_CONNECT;

  tr->ctx = create_ctx(&tr->opts);
  if (!tr->ctx) return TCONNECT_ERR_ALLOC;

  tr->ssl = SSL_new(tr->ctx);
  if (!tr->ssl) return TCONNECT_ERR_ALLOC;

  BIO *bio = make_transport_bio(tr->inner);
  if (!bio) return TCONNECT_ERR_ALLOC;
  SSL_set_bio(tr->ssl, bio, bio);

  const char *sni = tr->opts.sni_hostname ? tr->opts.sni_hostname : host;
  SSL_set_tlsext_host_name(tr->ssl, sni);
  SSL_set1_host(tr->ssl, sni);

  int ret = SSL_connect(tr->ssl);
  if (ret <= 0) {
    unsigned long e = ERR_get_error();
    tconnect_set_error("SSL_connect: %s",
                       e ? ERR_reason_error_string(e) : "unexpected EOF");
    return TCONNECT_ERR_CONNECT;
  }

  long verify = SSL_get_verify_result(tr->ssl);
  if (verify != X509_V_OK) {
    tconnect_set_error("certificate verification failed: %s",
                       X509_verify_cert_error_string(verify));
    return TCONNECT_ERR_CONNECT;
  }

  return TCONNECT_OK;
}

static int tls_read(transport_t *t, void *buf, size_t len) {
  if (!t) return -1;
  tls_transport_t *tr = (tls_transport_t *)t;
  return SSL_read(tr->ssl, buf, (int)len);
}

static int tls_write(transport_t *t, const void *buf, size_t len) {
  if (!t) return -1;
  tls_transport_t *tr = (tls_transport_t *)t;
  return SSL_write(tr->ssl, buf, (int)len);
}

static void tls_close(transport_t *t) {
  if (!t) return;
  tls_transport_t *tr = (tls_transport_t *)t;
  if (tr->ssl) { SSL_shutdown(tr->ssl); SSL_free(tr->ssl); tr->ssl = NULL; }
  if (tr->ctx) { SSL_CTX_free(tr->ctx); tr->ctx = NULL; }
  transport_close(tr->inner);
}

static int tls_get_fd(transport_t *t) {
  tls_transport_t *tr = (tls_transport_t *)t;
  return tr->inner->get_fd(tr->inner);
}

static void ensure_tls_init(void)
{
  static int initialized = 0;
  if (!initialized) {
    OPENSSL_init_ssl(0, NULL);
    initialized = 1;
  }
}

static transport_t *tls_alloc(transport_t *inner, tls_opts_t *opts) {
  tls_transport_t *tr = calloc(1, sizeof(tls_transport_t));
  if (!tr) return NULL;

  tr->inner        = inner;
  tr->base.connect = tls_connect;
  tr->base.read    = tls_read;
  tr->base.write   = tls_write;
  tr->base.close   = tls_close;
  tr->base.get_fd  = tls_get_fd;

  if (opts)
    tr->opts = *opts;
  else
    tr->opts.verify_peer = true;

  return (transport_t *)tr;
}

transport_t *tls_transport_create(tls_opts_t *opts)
{
  ensure_tls_init();
  return tls_alloc(tcp_transport_create(), opts);
}

transport_t *tls_transport_create_over(transport_t *inner, tls_opts_t *opts)
{
  ensure_tls_init();
  return tls_alloc(inner, opts);
}
