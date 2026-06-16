#include <stdlib.h>
#include "tconnect/async.h"

#ifdef __linux__
  #include <sys/epoll.h>
#else
  #include <poll.h>
#endif

#ifdef __linux__

struct tconnect_ctx_t {
  int          epfd;
  transport_t *transports[TCONNECT_MAX_CONNECTIONS];
  on_data_cb   callbacks[TCONNECT_MAX_CONNECTIONS];
  void        *userdata[TCONNECT_MAX_CONNECTIONS];
  int          nfds;
};

#else /* poll fallback */

struct tconnect_ctx_t {
  struct pollfd  fds[TCONNECT_MAX_CONNECTIONS];
  transport_t   *transports[TCONNECT_MAX_CONNECTIONS];
  on_data_cb     callbacks[TCONNECT_MAX_CONNECTIONS];
  void          *userdata[TCONNECT_MAX_CONNECTIONS];
  int            nfds;
};

#endif

tconnect_ctx_t *async_ctx_create(void) {
  tconnect_ctx_t *ctx = calloc(1, sizeof(tconnect_ctx_t));
  if (!ctx) return NULL;

#ifdef __linux__
  ctx->epfd = epoll_create1(0);
  if (ctx->epfd == -1) { free(ctx); return NULL; }
#endif

  return ctx;
}

void async_ctx_free(tconnect_ctx_t *ctx) {
  if (!ctx) return;
#ifdef __linux__
  close(ctx->epfd);
#endif
  free(ctx);
}

int async_register(tconnect_ctx_t *ctx, transport_t *t, on_data_cb cb, void *userdata) {
  if (ctx->nfds >= TCONNECT_MAX_CONNECTIONS) return -1;

  int fd = t->get_fd(t);
  if (fd == -1) return -1;

  int i = ctx->nfds;
  ctx->transports[i] = t;
  ctx->callbacks[i]  = cb;
  ctx->userdata[i]   = userdata;
  ctx->nfds++;

#ifdef __linux__
  struct epoll_event ev = {0};
  ev.events    = EPOLLIN;
  ev.data.ptr  = t;  /* get transport back directly in epoll_wait */
  epoll_ctl(ctx->epfd, EPOLL_CTL_ADD, fd, &ev);
#else
  ctx->fds[i].fd     = fd;
  ctx->fds[i].events = POLLIN;
#endif

  return 0;
}

void async_unregister(tconnect_ctx_t *ctx, transport_t *t) {
  for (int i = 0; i < ctx->nfds; i++) {
    if (ctx->transports[i] != t) continue;

#ifdef __linux__
    epoll_ctl(ctx->epfd, EPOLL_CTL_DEL, t->get_fd(t), NULL);
#endif

    int last = ctx->nfds - 1;
    ctx->transports[i] = ctx->transports[last];
    ctx->callbacks[i]  = ctx->callbacks[last];
    ctx->userdata[i]   = ctx->userdata[last];
#ifndef __linux__
    ctx->fds[i]        = ctx->fds[last];
#endif
    ctx->nfds--;
    return;
  }
}

int async_poll(tconnect_ctx_t *ctx, transport_t **ready, int max_ready, int timeout_ms) {
#ifdef __linux__

  struct epoll_event events[TCONNECT_MAX_CONNECTIONS];
  int n = epoll_wait(ctx->epfd, events, TCONNECT_MAX_CONNECTIONS, timeout_ms);
  if (n <= 0) return n;

  int count = 0;
  for (int i = 0; i < n && count < max_ready; i++) {
    transport_t *t = (transport_t *)events[i].data.ptr;

    for (int j = 0; j < ctx->nfds; j++) {
      if (ctx->transports[j] != t) continue;
      if (ctx->callbacks[j])
        ctx->callbacks[j](t, ctx->userdata[j]);
      if (ready) ready[count] = t;
      count++;
      break;
    }
  }
  return count;

#else /* poll fallback */

  int n = poll(ctx->fds, ctx->nfds, timeout_ms);
  if (n <= 0) return n;

  int count = 0;
  for (int i = 0; i < ctx->nfds && count < max_ready; i++) {
    if (!(ctx->fds[i].revents & POLLIN)) continue;
    if (ctx->callbacks[i])
      ctx->callbacks[i](ctx->transports[i], ctx->userdata[i]);
    if (ready) ready[count] = ctx->transports[i];
    count++;
  }
  return count;

#endif
}
