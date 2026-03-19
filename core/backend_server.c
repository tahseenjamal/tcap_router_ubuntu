#include <arpa/inet.h>
#include <netinet/sctp.h>
#include <osmocom/core/msgb.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../router/router.h"
#include "../router/tcap_parser.h"

#define MAX_EVENTS 1024
#define DIR_FROM_STP 0
#define DIR_FROM_BACKEND 1

static int listen_sock;

void backend_server_start(int port) {
    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));

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

                ev.events = EPOLLIN;
                ev.data.fd = client;

                epoll_ctl(ep, EPOLL_CTL_ADD, client, &ev);
                continue;
            }

            int fd = events[i].data.fd;

            uint8_t buf[4096];
            int r = recv(fd, buf, sizeof(buf), 0);

            if (r <= 0) {
                close(fd);
                continue;
            }

            struct msgb *msg = msgb_alloc(r, "backend");
            memcpy(msg->data, buf, r);
            msg->len = r;

            /* parse TCAP */
            uint32_t otid = 0, dtid = 0;
            int type = 0;

            /* reuse your parser */
            parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

            msg->cb[0] = DIR_FROM_BACKEND;
            route_tcap(msg, otid, dtid, type);
        }
    }
}

