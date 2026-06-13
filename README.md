# tconnect - tiny connect

A small, embeddable C networking library for TCP/UDP clients with HTTP and WebSocket support.

## Features

- TCP client with sync and async (poll-based) I/O
- Pluggable transport layer - swap TCP for TLS or UDP without changing application code
- Accumulation buffer per connection for stream parsing
- Callback and inline async models
- Linux-first (epoll), macOS dev supported (poll fallback), Windows planned

## Building

```bash
cmake -B build -S .
cmake --build build
```

## Usage

### Sync TCP

```c
transport_t *t = tcp_transport_create();
t->connect(t, "example.com", "80");

char buf[4096] = {0};
transport_read(t, buf, sizeof(buf));

t->close(t);
```

### Async (poll-based)

```c
void on_data(transport_t *t, void *userdata) {
    connection_t *c = (connection_t *)userdata;
    connection_recv(c);
    printf("%.*s", (int)c->buf_len, c->buf);
    connection_consume(c, c->buf_len);
}

transport_t *t  = tcp_transport_create();
t->connect(t, "example.com", "80");

connection_t    *c   = connection_create(t, 1);
tconnect_ctx_t  *ctx = async_ctx_create();
async_register(ctx, t, on_data, c);

while (1)
    async_poll(ctx, NULL, 0, -1);
```

## Architecture

```
[ WebSocket ]  - planned
[ HTTP/1.1  ]  - planned
[ TLS       ]  - planned (OpenSSL)
[ TCP / UDP ]  - transport_t abstraction
[ OS layer  ]  - epoll (Linux) / poll fallback (macOS) / IOCP (Windows, planned)
```

## Roadmap

- [ ] HTTP/1.1 request and response parser
- [ ] WebSocket handshake and frame parser
- [ ] TLS via OpenSSL
- [ ] UDP transport
- [ ] epoll backend (Linux)
- [ ] Windows (Winsock2 + IOCP)
