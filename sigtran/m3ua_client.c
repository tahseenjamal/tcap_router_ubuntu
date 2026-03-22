#include "m3ua.h"

#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int stp_sock = -1;

/* ===================================== */

int m3ua_client_connect(const char *ip, int port) {
    stp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (stp_sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(stp_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return -1;
    }

    printf("Connected to STP %s:%d\n", ip, port);
    return 0;
}

/* ===================================== */

static int build_m3ua(uint8_t *out, uint8_t *sccp, int sccp_len) {
    m3ua_hdr_t *h = (m3ua_hdr_t *)out;

    h->version = 1;
    h->reserved = 0;
    h->msg_class = M3UA_CLASS_TRANSFER;
    h->msg_type = M3UA_DATA;
    h->length = htonl(sizeof(m3ua_hdr_t) + sccp_len);

    memcpy(out + sizeof(m3ua_hdr_t), sccp, sccp_len);

    return sizeof(m3ua_hdr_t) + sccp_len;
}

/* ===================================== */

int m3ua_send(uint8_t *sccp, int len) {
    if (stp_sock < 0) return -1;

    uint8_t buf[4096];

    int mlen = build_m3ua(buf, sccp, len);

    return send(stp_sock, buf, mlen, 0);
}

/* ===================================== */

int m3ua_recv(uint8_t *out, int max_len) {
    return recv(stp_sock, out, max_len, 0);
}
