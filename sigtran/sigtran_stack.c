#include "sigtran_stack.h"

#include <osmocom/sigtran/osmo_ss7.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/prim.h>

#include <osmocom/sigtran/osmo_ss7.h>
#include <osmocom/sigtran/sccp_sap.h>

#include <stdint.h>
#include <stdio.h>

#include "../core/worker_pool.h"

static struct osmo_ss7_instance *ss7;
static struct osmo_sccp_instance *sccp;

/* Extract OTID for worker hashing */

static uint32_t extract_otid(uint8_t *d, int len)
{
    if (len < 2)
        return 0;

    for (int i = 0; i < len - 6; i++) {

        if (d[i] == 0x48) {

            int l = d[i + 1];

            if (l > 0 && l <= 4 && i + 2 + l <= len) {

                uint32_t id = 0;

                for (int j = 0; j < l; j++)
                    id = (id << 8) | d[i + 2 + j];

                return id;
            }
        }
    }

    return 0;
}

/* SCCP callback */

static int sccp_prim_cb(struct osmo_prim_hdr *oph, void *ctx)
{
    if (!oph)
        return 0;

    if (oph->primitive != OSMO_SCU_PRIM_N_DATA)
        return 0;

    struct msgb *msg = oph->msg;

    if (!msg)
        return 0;

    uint32_t otid = extract_otid(msg->data, msg->len);

    worker_enqueue(msg, otid, 0, 0);

    return 0;
}

/* Initialize SIGTRAN / SCCP */

void sigtran_start()
{
    printf("Initializing SS7 + SCCP stack\n");

    /* initialize SS7 subsystem */
    osmo_ss7_init();

    ss7 = osmo_ss7_instance_find_or_create(NULL, 0);

    if (!ss7) {
        printf("Failed to create SS7 instance\n");
        return;
    }

    sccp = osmo_sccp_instance_create(ss7, NULL);

    if (!sccp) {
        printf("Failed to create SCCP instance\n");
        return;
    }

    osmo_sccp_user_bind(sccp, "tcap-router", sccp_prim_cb, 146);

    printf("SCCP stack initialized\n");
}
