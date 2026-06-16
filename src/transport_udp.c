#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "tconnect/transport.h"

typedef struct {
  transport_t base;
  int         fd;
} udp_transport_t;

static int  udp_connect(transport_t *t, const char *host, const char* port) {
  udp_transport_t *udp = (udp_transport_t *)t; 
  struct addrinfo hints = {0};
  hints.ai_family   = AF_UNSPEC; // IPv4
  hints.ai_socktype = SOCK_DGRAM; // udp 

  struct addrinfo *result;
  int err = getaddrinfo(host, port, &hints, &result);
  if (err != 0) {
    tconnect_set_error("getaddrinfo: %s", gai_strerror(err));
    return TCONNECT_ERR_CONNECT;
  }

  int client_socket = -1;
  struct addrinfo *addr;
  for (addr = result; addr != NULL; addr = addr->ai_next) {
    client_socket = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (client_socket == -1)
      continue;

    if (connect(client_socket, addr->ai_addr, addr->ai_addrlen) == 0)
      break;

    close(client_socket);
    client_socket = -1;
  }

  udp->fd = client_socket; 
  freeaddrinfo(result);

  if (client_socket == -1) {
    tconnect_set_error("could not connect to %s:%s", host, port);
    return TCONNECT_ERR_CONNECT;
  }
  return TCONNECT_OK;
}

static int  udp_read(transport_t *t, void *buf, size_t len) {
  if(t == NULL) return -1;
  udp_transport_t *udp = (udp_transport_t *)t;
  return recvfrom(udp->fd, buf, len, 0, NULL, 0);
}

static int  udp_write(transport_t *t, const void *buf, size_t len) { 
 if(t == NULL) return -1;
 udp_transport_t *udp = (udp_transport_t *)t;
 return sendto(udp->fd, buf, len, 0, NULL, 0);
}

static void udp_close(transport_t *t) { 
  if(t == NULL) return;
  udp_transport_t *udp = (udp_transport_t *)t;
  close(udp->fd);
  return;
}

static int udp_get_fd(transport_t *t){ 
  if(t == NULL) return -1;
  udp_transport_t *udp = (udp_transport_t *)t;
  return udp->fd; 
}

static transport_t *udp_alloc(void) {
  udp_transport_t *udp = malloc(sizeof(udp_transport_t));
  if (!udp) return NULL;

  udp->fd           = -1;
  udp->base.connect = udp_connect;
  udp->base.read    = udp_read;
  udp->base.write   = udp_write;
  udp->base.close   = udp_close;
  udp->base.get_fd  = udp_get_fd;

  return (transport_t *)udp;
}

transport_t *udp_transport_create(void) {
  return udp_alloc();
}

transport_t *udp_transport_create_opts(transport_opts_t *opts) {
  transport_t *t = udp_alloc();
  if (!t || !opts) return t;

  udp_transport_t *udp = (udp_transport_t *)t;

  if (opts->recv_buf_size > 0)
    setsockopt(udp->fd, SOL_SOCKET, SO_RCVBUF,
               &opts->recv_buf_size, sizeof(opts->recv_buf_size));

  if (opts->send_buf_size > 0)
    setsockopt(udp->fd, SOL_SOCKET, SO_SNDBUF,
               &opts->send_buf_size, sizeof(opts->send_buf_size));

  return t;
}
