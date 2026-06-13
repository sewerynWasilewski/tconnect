#ifndef TCONNECT_TRANSPORT_H
#define TCONNECT_TRANSPORT_H

#include <stddef.h>

typedef struct transport_t transport_t;

struct transport_t {
  int  (*connect)(transport_t *t, const char *host, char const *port);
  int  (*read)   (transport_t *t, void *buf, size_t len);
  int  (*write)  (transport_t *t, const void *buf, size_t len);
  void (*close)  (transport_t *t);

  /* returns the underlying fd for use with epoll/select.
   * return -1 if this transport has no fd (event loop will skip it) */
  int (*get_fd)(transport_t *t);
};

transport_t *tcp_transport_create(void);
void         tcp_transport_free(transport_t *t);

#endif
