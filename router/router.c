#include "router.h"

#include <osmocom/core/msgb.h>
#include <osmocom/core/prim.h>
#include <osmocom/sigtran/sccp_sap.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/socket.h>

#include "../core/backend_pool.h"
#include "../core/transaction_table.h"
#include "sccp_gt.h"

#define DIR_FROM_STP 0
#define DIR_FROM_BACKEND 1

extern struct osmo_sccp_user *sccp_user;

/* Provided by sigtran layer */
extern struct osmo_sccp_instance *sccp;

/*
 * Self GT (used when forwarding to network)
 */
static uint8_t SELF_GT[] = {0x91, 0x88, 0x77, 0x66};
static int SELF_GT_LEN = 4;

/*
 * Extract SCCP user data (TCAP payload)
 */
static uint8_t *extract_sccp_userdata(struct msgb *msg, int *len) {
    uint8_t *d = msg->data;

    if (msg->len < 5) return NULL;

    uint8_t msg_type = d[0];

    /* UDT / XUDT / LUDT */
    if (msg_type != 0x09 && msg_type != 0x11 && msg_type != 0x13) return NULL;

    uint8_t ptr_data = d[3];

    if (ptr_data == 0 || ptr_data >= msg->len) return NULL;

    int data_offset = ptr_data + 1;

    if (data_offset >= msg->len) return NULL;

    *len = msg->len - data_offset;

    return &d[data_offset];
}

/*
 * Send message to STP via SCCP
 */

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

/*
 * Send message to backend
 */
static void send_to_backend(backend_t *b, struct msgb *msg) {
    atomic_fetch_add(&b->load, 1);

    ssize_t sent = send(b->sock, msg->data, msg->len, MSG_NOSIGNAL);

    atomic_fetch_sub(&b->load, 1);

    if (sent <= 0) {
        atomic_store(&b->active, 0);
    }

    msgb_free(msg);
}

/*
 * Main routing function
 * direction = 0 → from STP
 * direction = 1 → from backend
 */
void route_tcap(struct msgb *msg, uint32_t otid, uint32_t dtid, int type) {
    int tcap_len = 0;
    uint8_t *tcap = extract_sccp_userdata(msg, &tcap_len);

    if (!tcap) {
        msgb_free(msg);
        return;
    }

    backend_t *b = NULL;

    int is_begin = (type == 1 && otid != 0);

    /* ============================================
     * CASE 1: MESSAGE FROM STP → BACKEND
     * ============================================ */
    int direction = msg->cb[0];
    if (direction == DIR_FROM_STP) {
        if (is_begin) {
            /* New dialog */

            b = backend_choose();

            if (!b) {
                msgb_free(msg);
                return;
            }

            uint8_t orig_gt[32];
            int gt_len = 0;

            if (extract_calling_gt(msg->data, msg->len, orig_gt, &gt_len) < 0)
                gt_len = 0;

            /* Rewrite GT → SELF */
            rewrite_calling_gt(msg->data, msg->len, SELF_GT, SELF_GT_LEN);

            tx_store_full(otid, b->id, orig_gt, gt_len);

        } else {
            /* Existing dialog */

            if (!dtid) {
                msgb_free(msg);
                return;
            }

            tx_info_t info;

            if (tx_lookup_full(dtid, &info) < 0) {
                msgb_free(msg);
                return;
            }

            b = backend_get(info.backend);

            if (!b) {
                msgb_free(msg);
                return;
            }

            if (info.gt_len > 0) {
                rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);
            }

            if (type == 3) tx_delete(dtid);
        }

        send_to_backend(b, msg);
        return;
    }

    /* ============================================
     * CASE 2: MESSAGE FROM BACKEND → STP
     * ============================================ */

    if (is_begin) {
        /* Backend initiating dialog */

        uint8_t orig_gt[32];
        int gt_len = 0;

        if (extract_calling_gt(msg->data, msg->len, orig_gt, &gt_len) < 0)
            gt_len = 0;

        rewrite_calling_gt(msg->data, msg->len, SELF_GT, SELF_GT_LEN);

        tx_store_full(otid, -1, orig_gt, gt_len);

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

        if (type == 3) tx_delete(dtid);
    }

    send_to_stp(msg);
}
