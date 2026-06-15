#ifndef TCONNECT_ASYNC_H
#define TCONNECT_ASYNC_H

#include "transport.h"
#include <stddef.h>

#define TCONNECT_MAX_CONNECTIONS 64

typedef struct tconnect_ctx_t tconnect_ctx_t;
typedef void (*on_data_cb)(transport_t *t, void *userdata);

tconnect_ctx_t *async_ctx_create(void);
void            async_ctx_free(tconnect_ctx_t *ctx);
int             async_register(tconnect_ctx_t *ctx, transport_t *t, on_data_cb cb, void *userdata);
void            async_unregister(tconnect_ctx_t *ctx, transport_t *t);
int             async_poll(tconnect_ctx_t *ctx, transport_t **ready, int max_ready, int timeout_ms);

#endif
