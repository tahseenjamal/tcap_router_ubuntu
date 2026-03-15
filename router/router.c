#include "router.h"

#include <osmocom/core/msgb.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/socket.h>

#include "../core/backend_pool.h"
#include "../core/transaction_table.h"

/*
 * Extract SCCP user data (TCAP payload)
 */

static uint8_t* extract_sccp_userdata(struct msgb* msg, int* len) {
    uint8_t* d = msg->data;

    if (msg->len < 5) return NULL;

    uint8_t msg_type = d[0];

    /* Only handle UDT / XUDT */

    if (msg_type != 0x09 && msg_type != 0x11) return NULL;

    uint8_t ptr_data = d[3];

    int data_offset = ptr_data + 1;

    if (data_offset >= msg->len) return NULL;

    *len = msg->len - data_offset;

    return &d[data_offset];
}

/*
 * Fast TCAP parser
 * Extracts OTID / DTID and message type
 */

static void parse_tcap(uint8_t* d, int len, uint32_t* otid, uint32_t* dtid,
                       int* type) {
    *otid = 0;
    *dtid = 0;
    *type = 0;

    if (len < 2) return;

    /* TCAP message type */

    switch (d[0]) {
        case 0x62:
            *type = 1;
            break; /* Begin */
        case 0x65:
            *type = 2;
            break; /* Continue */
        case 0x64:
            *type = 3;
            break; /* End */
        default:
            return;
    }

    /* Scan for OTID / DTID */

    for (int i = 0; i < len - 6; i++) {
        /* OTID */

        if (d[i] == 0x48) {
            int l = d[i + 1];

            if (l > 0 && l <= 4 && i + 2 + l <= len) {
                uint32_t id = 0;

                for (int j = 0; j < l; j++) id = (id << 8) | d[i + 2 + j];

                *otid = id;
            }
        }

        /* DTID */

        if (d[i] == 0x49) {
            int l = d[i + 1];

            if (l > 0 && l <= 4 && i + 2 + l <= len) {
                uint32_t id = 0;

                for (int j = 0; j < l; j++) id = (id << 8) | d[i + 2 + j];

                *dtid = id;
            }
        }
    }
}

/*
 * TCAP routing logic
 */

void route_tcap(struct msgb* msg, uint32_t otid, uint32_t dtid, int type) {
    backend_t* b = NULL;

    int tcap_len = 0;

    uint8_t* tcap = extract_sccp_userdata(msg, &tcap_len);

    if (!tcap) {
        msgb_free(msg);
        return;
    }

    /* Parse TCAP payload */

    parse_tcap(tcap, tcap_len, &otid, &dtid, &type);

    /*
     * Dialog routing
     *
     * Begin → select backend
     * Continue / End → lookup DTID
     */

    if (type == 1) {
        /* Begin */

        b = backend_choose();

        if (b && otid) tx_store(otid, b->id);
    } else {
        /* Continue / End */

        int backend = tx_lookup(dtid);

        b = backend_get(backend);
    }

    if (!b) {
        msgb_free(msg);
        return;
    }

    /*
     * Forward message
     */

    atomic_fetch_add(&b->load, 1);

    ssize_t sent = send(b->sock, msg->data, msg->len, MSG_NOSIGNAL);

    atomic_fetch_sub(&b->load, 1);

    if (sent <= 0) {
        /* backend likely failed */

        b->active = 0;
    }

    msgb_free(msg);
}
