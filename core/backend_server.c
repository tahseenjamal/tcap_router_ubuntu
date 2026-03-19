#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <osmocom/core/msgb.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../router/router.h"
#include "../router/tcap_parser.h"

#define MAX_EVENTS 1024
#define MAX_BACKENDS 1024
#define DIR_FROM_STP 0
#define DIR_FROM_BACKEND 1

int backend_fds[MAX_BACKENDS];
atomic_int backend_count = 0;
atomic_int rr = 0;

/* ========================================= */

static void add_backend(int fd) {
    int idx = atomic_fetch_add(&backend_count, 1);

    if (idx >= MAX_BACKENDS) {
        close(fd);
        atomic_fetch_sub(&backend_count, 1);
        return;
    }

    backend_fds[idx] = fd;
}

/* ========================================= */

static void remove_backend(int fd) {
    int n = atomic_load(&backend_count);

    for (int i = 0; i < n; i++) {
        if (backend_fds[i] == fd) {
            backend_fds[i] = backend_fds[n - 1];
            atomic_fetch_sub(&backend_count, 1);
            return;
        }
    }
}

/* ========================================= */

void backend_server_start(int port) {
    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_sock, 128);

    int ep = epoll_create1(0);

    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.fd = listen_sock;

    epoll_ctl(ep, EPOLL_CTL_ADD, listen_sock, &ev);

    while (1) {
        struct epoll_event events[MAX_EVENTS];
        int n = epoll_wait(ep, events, MAX_EVENTS, -1);

        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == listen_sock) {
                int client = accept(listen_sock, NULL, NULL);
                add_backend(client);

                ev.events = EPOLLIN;
                ev.data.fd = client;

                epoll_ctl(ep, EPOLL_CTL_ADD, client, &ev);
                continue;
            }

            int fd = events[i].data.fd;

            uint8_t buf[4096];
            int r = recv(fd, buf, sizeof(buf), 0);

            if (r <= 0) {
                epoll_ctl(ep, EPOLL_CTL_DEL, fd, NULL);
                remove_backend(fd);
                close(fd);
                continue;
            }

            struct msgb *msg = msgb_alloc(r, "backend");

            memcpy(msg->data, buf, r);
            msg->len = r;

            uint32_t otid = 0, dtid = 0;
            int type = 0;

            parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

            msg->cb[0] = DIR_FROM_BACKEND;
            memcpy(&msg->cb[1], &fd, sizeof(int));

            route_tcap(msg, otid, dtid, type);
        }
    }
}
