#include "router.h"

#include <osmocom/core/msgb.h>
#include <osmocom/core/prim.h>
#include <osmocom/sigtran/sccp_sap.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../core/transaction_table.h"
#include "sccp_gt.h"

#define DIR_FROM_STP 0
#define DIR_FROM_BACKEND 1

/* Provided by sigtran */
extern struct osmo_sccp_user *sccp_user;

/* backend registry (from backend_server.c) */
extern int backend_fds[];
extern atomic_int backend_count;
extern atomic_int rr;

/* ============================================
 * Self GT (rewrite when going to backend)
 * ============================================ */
static uint8_t SELF_GT[] = {0x91, 0x88, 0x77, 0x66};
static int SELF_GT_LEN = 4;

/* ============================================
 * Backend selection
 * ============================================ */
static int choose_backend_fd() {
    int n = atomic_load(&backend_count);

    if (n <= 0) return -1;

    int i = atomic_fetch_add(&rr, 1);

    return backend_fds[i % n];
}

/* ============================================
 * Extract backend fd from msg
 * ============================================ */
static inline int get_backend_fd(struct msgb *msg) {
    int fd = -1;
    memcpy(&fd, &msg->cb[1], sizeof(int));
    return fd;
}

/* ============================================
 * Extract SCCP payload (TCAP)
 * ============================================ */
static uint8_t *extract_sccp_userdata(struct msgb *msg, int *len) {
    uint8_t *d = msg->data;

    if (msg->len < 5) return NULL;

    uint8_t msg_type = d[0];

    /* UDT / XUDT / LUDT */
    if (msg_type != 0x09 && msg_type != 0x11 && msg_type != 0x13) return NULL;

    uint8_t ptr_data = d[3];

    if (ptr_data == 0 || ptr_data >= msg->len) return NULL;

    int offset = ptr_data + 1;

    if (offset >= msg->len) return NULL;

    *len = msg->len - offset;

    return &d[offset];
}

/* ============================================
 * Send to STP
 * ============================================ */
static void send_to_stp(struct msgb *msg) {
    if (!sccp_user) {
        msgb_free(msg);
        return;
    }

    struct osmo_prim_hdr oph;
    memset(&oph, 0, sizeof(oph));

    oph.primitive = OSMO_SCU_PRIM_N_DATA;
    oph.operation = PRIM_OP_REQUEST;
    oph.msg = msg;

    osmo_sccp_user_sap_down(sccp_user, &oph);
}

/* ============================================
 * Send to backend (safe)
 * ============================================ */
static void send_to_backend_fd(int fd, struct msgb *msg) {
    ssize_t s = send(fd, msg->data, msg->len, MSG_NOSIGNAL);

    if (s <= 0) {
        printf("Backend send failed fd=%d\n", fd);
        close(fd);
        msgb_free(msg);
        return;
    }

    msgb_free(msg);
}

/* ============================================
 * MAIN ROUTER
 * ============================================ */
void route_tcap(struct msgb *msg, uint32_t otid, uint32_t dtid, int type) {
    int tcap_len = 0;

    uint8_t *tcap = extract_sccp_userdata(msg, &tcap_len);

    if (!tcap) {
        msgb_free(msg);
        return;
    }

    int direction = msg->cb[0];
    int is_begin = (type == 1 && otid != 0);

    /* ============================================
     * STP → BACKEND
     * ============================================ */
    if (direction == DIR_FROM_STP) {
        int backend_fd = -1;

        if (is_begin) {
            /* NEW DIALOG */

            backend_fd = choose_backend_fd();

            if (backend_fd < 0) {
                printf("No backend available\n");
                msgb_free(msg);
                return;
            }

            /* Extract original GT */
            uint8_t orig_gt[32];
            int gt_len = 0;

            if (extract_calling_gt(msg->data, msg->len, orig_gt, &gt_len) < 0) {
                gt_len = 0;
            }

            /* Rewrite to SELF GT */
            rewrite_calling_gt(msg->data, msg->len, SELF_GT, SELF_GT_LEN);

            /* Store dialog */
            tx_store_full(otid, backend_fd, orig_gt, gt_len);

        } else {
            /* EXISTING DIALOG */

            if (!dtid) {
                msgb_free(msg);
                return;
            }

            tx_info_t info;

            if (tx_lookup_full(dtid, &info) < 0) {
                msgb_free(msg);
                return;
            }

            backend_fd = info.backend;

            /* Restore original GT */
            if (info.gt_len > 0) {
                rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);
            }

            if (type == 3) { /* END */
                tx_delete(dtid);
            }
        }

        send_to_backend_fd(backend_fd, msg);
        return;
    }

    /* ============================================
     * BACKEND → STP
     * ============================================ */

    int backend_fd = get_backend_fd(msg);

    if (backend_fd < 0) {
        msgb_free(msg);
        return;
    }

    if (is_begin) {
        /* Backend initiated */

        uint8_t orig_gt[32];
        int gt_len = 0;

        if (extract_calling_gt(msg->data, msg->len, orig_gt, &gt_len) < 0) {
            gt_len = 0;
        }

        rewrite_calling_gt(msg->data, msg->len, SELF_GT, SELF_GT_LEN);

        tx_store_full(otid, backend_fd, orig_gt, gt_len);

    } else {
        if (!dtid) {
            msgb_free(msg);
            return;
        }

        tx_info_t info;

        if (tx_lookup_full(dtid, &info) < 0) {
            msgb_free(msg);
            return;
        }

        if (info.gt_len > 0) {
            rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);
        }

        if (type == 3) {
            tx_delete(dtid);
        }
    }

    send_to_stp(msg);
}
