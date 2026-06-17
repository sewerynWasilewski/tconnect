# tconnect - tiny connect

A small, embeddable C networking library for TCP/UDP clients with HTTP/HTTPS and WebSocket support.

## Features

- TCP and UDP transports via `transport_t` abstraction
- TLS via OpenSSL - wraps any transport as a decorator
- HTTP/1.1 client - GET, POST, redirects, custom headers
- HTTPS - automatic TLS selection from URL scheme
- Async I/O - epoll (Linux) / poll (macOS) with callback and inline models
- Thread-local error strings - `tconnect_last_error()` always has context on failure
- `transport_read_exact()` - loop helper for fixed-size reads (WebSocket frames etc.)

## Building

Requires CMake 3.15+ and OpenSSL.

```bash
cmake -B build -S .
cmake --build build
```

## Usage

See the `examples/` directory for complete, buildable examples.

## Architecture

```
[ HTTP/1.1  ]  - GET, POST, redirects, HTTPS
[ TLS       ]  - OpenSSL, decorator over any transport
[ TCP / UDP ]  - transport_t abstraction
[ OS layer  ]  - epoll (Linux) / poll (macOS)
```

## Roadmap

- [ ] WebSocket handshake and frame parser
- [ ] HTTP/2 (requires TLS + ALPN)
- [ ] Windows (Winsock2)
