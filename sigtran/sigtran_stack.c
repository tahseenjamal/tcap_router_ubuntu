#include "sigtran_stack.h"

#include <osmocom/sigtran/osmo_ss7.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/prim.h>
#include <osmocom/sigtran/sccp_sap.h>

#include <stdint.h>
#include <stdio.h>

#include "../core/worker_pool.h"

static struct osmo_ss7_instance *ss7;
static struct osmo_sccp_instance *sccp;

/* ------------------------------------------------ */
/* Fast TCAP parser                                 */
/* ------------------------------------------------ */

static void parse_tcap(uint8_t *d, int len,
                       uint32_t *otid,
                       uint32_t *dtid,
                       int *type)
{
    *otid = 0;
    *dtid = 0;
    *type = 0;

    if (len < 2)
        return;

    switch (d[0]) {

    case 0x62:
        *type = 1;
        break;

    case 0x65:
        *type = 2;
        break;

    case 0x64:
        *type = 3;
        break;

    default:
        return;
    }

    for (int i = 0; i < len - 6; i++) {

        if (d[i] == 0x48) {

            int l = d[i + 1];

            if (l > 0 && l <= 4 && i + 2 + l <= len) {

                uint32_t id = 0;

                for (int j = 0; j < l; j++)
                    id = (id << 8) | d[i + 2 + j];

                *otid = id;
            }
        }

        if (d[i] == 0x49) {

            int l = d[i + 1];

            if (l > 0 && l <= 4 && i + 2 + l <= len) {

                uint32_t id = 0;

                for (int j = 0; j < l; j++)
                    id = (id << 8) | d[i + 2 + j];

                *dtid = id;
            }
        }
    }
}

/* ------------------------------------------------ */
/* SCCP callback                                    */
/* ------------------------------------------------ */

static int sccp_prim_cb(struct osmo_prim_hdr *oph, void *ctx)
{
    if (!oph)
        return 0;

    if (oph->primitive != OSMO_SCU_PRIM_N_DATA)
        return 0;

    struct msgb *msg = oph->msg;

    if (!msg)
        return 0;

    uint32_t otid = 0;
    uint32_t dtid = 0;
    int type = 0;

    parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

    worker_enqueue(msg, otid, dtid, type);

    return 0;
}

/* ------------------------------------------------ */
/* Initialize SIGTRAN / SCCP                        */
/* ------------------------------------------------ */

void sigtran_start()
{
    printf("Initializing SS7 + SCCP stack\n");

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
