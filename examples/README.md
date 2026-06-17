# examples

Each file is a self-contained program. Build with the main project:

```bash
cmake -B build -S ..
cmake --build build
```

Binaries land in `build/examples/`.

| Example | Binary | What it shows |
|---------|--------|---------------|
| `https_get.c` | `example_https_get` | HTTPS GET - TLS selected automatically from `https://` URL |
| `http_get_post.c` | `example_http_get_post` | HTTP GET, POST with JSON body, redirect following |
| `tls_raw.c` | `example_tls_raw` | Raw TLS transport without the HTTP layer |
| `udp_dns.c` | `example_udp_dns` | UDP DNS query to 8.8.8.8:53, parses A records from response |

## Running

```bash
./build/examples/example_https_get
./build/examples/example_http_get_post
./build/examples/example_tls_raw
./build/examples/example_udp_dns
```
