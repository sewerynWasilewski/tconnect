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
} tcp_transport_t;

static int  tcp_connect(transport_t *t, const char *host, const char* port) {
  tcp_transport_t *tcp = (tcp_transport_t *)t; 
  struct addrinfo hints = {0};
  hints.ai_family   = AF_UNSPEC; // IPv4
  hints.ai_socktype = SOCK_STREAM; // TCP 

  struct addrinfo *result;
  int err = getaddrinfo(host, port, &hints, &result);
  if (err != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    return 1;
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

  tcp->fd = client_socket; 
  freeaddrinfo(result);

  if (client_socket == -1) {
    fprintf(stderr, "could not connect to %s:%s\n", host, port);
    return 1;
  }
  return 0;
}

static int  tcp_read(transport_t *t, void *buf, size_t len) {
  if(t == NULL) return -1;
  tcp_transport_t *tcp = (tcp_transport_t *)t;
  return recv(tcp->fd, buf, len - 1, 0);
}

static int  tcp_write(transport_t *t, const void *buf, size_t len) { 
 if(t == NULL) return -1;
 tcp_transport_t *tcp = (tcp_transport_t *)t;
 return send(tcp->fd, buf, len, 0);
}

static void tcp_close(transport_t *t) { 
  if(t == NULL) return;
  tcp_transport_t *tcp = (tcp_transport_t *)t;
  close(tcp->fd);
  return;
}

static int tcp_get_fd(transport_t *t){ 
  if(t == NULL) return -1;
  tcp_transport_t *tcp = (tcp_transport_t *)t;
  return tcp->fd; 
}

transport_t *tcp_transport_create(void)
{
  tcp_transport_t *tcp = malloc(sizeof(tcp_transport_t));
  if (!tcp) return NULL;

  tcp->fd           = -1;
  tcp->base.connect = tcp_connect;
  tcp->base.read    = tcp_read;
  tcp->base.write   = tcp_write;
  tcp->base.close   = tcp_close;
  tcp->base.get_fd  = tcp_get_fd; 

  return (transport_t *)tcp;  // safe - base is first member
}
