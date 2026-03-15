#include "sctp_server.h"

#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <osmocom/core/msgb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../core/worker_pool.h"

#define MAX_EVENTS 128
#define BUFFER_SIZE 4096

static int server;

/* extract OTID */

static uint32_t extract_otid(uint8_t* d, int len) {
    if (len < 2) return 0;

    for (int i = 0; i < len - 6; i++) {
        if (d[i] == 0x48) {
            int l = d[i + 1];

            if (l > 0 && l <= 4 && i + 2 + l <= len) {
                uint32_t id = 0;

                for (int j = 0; j < l; j++) id = (id << 8) | d[i + 2 + j];

                return id;
            }
        }
    }

    return 0;
}

static void process_packet(int fd) {
    uint8_t buffer[BUFFER_SIZE];

    int n = recv(fd, buffer, sizeof(buffer), 0);

    if (n <= 0) return;

    struct msgb* msg = msgb_alloc(n, "sccp");

    if (!msg) return;

    memcpy(msgb_put(msg, n), buffer, n);

    uint32_t otid = extract_otid(buffer, n);

    worker_enqueue(msg, otid, 0, 0);
}

/* start server */

void sctp_server_start(int port) {
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server, (struct sockaddr*)&addr, sizeof(addr));

    listen(server, 128);

    int ep = epoll_create1(0);

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];

    ev.events = EPOLLIN;
    ev.data.fd = server;

    epoll_ctl(ep, EPOLL_CTL_ADD, server, &ev);

    printf("SCTP server listening on %d\n", port);

    while (1) {
        int n = epoll_wait(ep, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == server) {
                int client = accept(server, NULL, NULL);

                ev.events = EPOLLIN;
                ev.data.fd = client;

                epoll_ctl(ep, EPOLL_CTL_ADD, client, &ev);
            } else {
                process_packet(fd);
            }
        }
    }
}
