#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tconnect/transport.h"
#include "tconnect/http.h"

static int skip_dns_name(const unsigned char *buf, int pos, int n) {
  if (pos < n && (buf[pos] & 0xc0) == 0xc0) return pos + 2;
  while (pos < n && buf[pos]) pos += buf[pos] + 1;
  return pos + 1;
}

static void test_udp_dns(void) {
  printf("=== UDP (DNS query for google.com via 8.8.8.8:53) ===\n");

  /* minimal DNS query: ask for A record of google.com */
  static const unsigned char query[] = {
    0x00, 0x01,              /* transaction ID */
    0x01, 0x00,              /* flags: standard query, recursion desired */
    0x00, 0x01,              /* questions: 1 */
    0x00, 0x00,              /* answer RRs: 0 */
    0x00, 0x00,              /* authority RRs: 0 */
    0x00, 0x00,              /* additional RRs: 0 */
    0x06, 'g','o','o','g','l','e',  /* \x06google */
    0x03, 'c','o','m',       /* \x03com */
    0x00,                    /* root label */
    0x00, 0x01,              /* QTYPE: A */
    0x00, 0x01,              /* QCLASS: IN */
  };

  transport_t *t = udp_transport_create();
  if (!t) { fprintf(stderr, "udp alloc failed\n"); return; }

  if (transport_connect(t, "8.8.8.8", "53") != TCONNECT_OK) {
    fprintf(stderr, "udp connect failed: %s\n", tconnect_last_error());
    transport_close(t);
    return;
  }

  transport_write(t, query, sizeof(query));

  unsigned char buf[512];
  int n = transport_read(t, buf, sizeof(buf));
  if (n < 12) {
    fprintf(stderr, "udp read failed or response too short\n");
  } else {
    int answers = (buf[6] << 8) | buf[7];
    printf("received %d bytes, %d answer(s):\n", n, answers);

    /* skip header (12 bytes) + question section */
    int pos = skip_dns_name(buf, 12, n) + 4;  /* name + QTYPE + QCLASS */

    for (int i = 0; i < answers && pos + 12 <= n; i++) {
      pos = skip_dns_name(buf, pos, n);

      int type     = (buf[pos] << 8) | buf[pos + 1];
      pos += 8;  /* type + class + ttl */
      int rdlength = (buf[pos] << 8) | buf[pos + 1];
      pos += 2;

      if (type == 1 && rdlength == 4 && pos + 4 <= n)
        printf("  A: %d.%d.%d.%d\n", buf[pos], buf[pos+1], buf[pos+2], buf[pos+3]);

      pos += rdlength;
    }
  }

  transport_close(t);
  free(t);
}

int main(void) {
  test_udp_dns();
  return 0;
}
