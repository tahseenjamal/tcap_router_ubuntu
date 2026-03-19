#include "sigtran_stack.h"

#include <osmocom/core/msgb.h>
#include <osmocom/core/prim.h>
#include <osmocom/sigtran/osmo_ss7.h>
#include <osmocom/sigtran/sccp_sap.h>
#include <stdint.h>
#include <stdio.h>

#include "../core/worker_pool.h"
#include "../router/tcap_parser.h"

#define DIR_FROM_STP 0
#define DIR_FROM_BACKEND 1

static struct osmo_ss7_instance *ss7;
struct osmo_sccp_instance *sccp;
struct osmo_sccp_user *sccp_user;

/* SCCP callback                                    */
/* ------------------------------------------------ */

static int sccp_prim_cb(struct osmo_prim_hdr *oph, void *ctx) {
    if (!oph) return 0;

    if (oph->primitive != OSMO_SCU_PRIM_N_DATA) return 0;

    struct msgb *msg = oph->msg;

    if (!msg) return 0;

    uint32_t otid = 0;
    uint32_t dtid = 0;
    int type = 0;

    parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

    msg->cb[0] = DIR_FROM_STP;
    worker_enqueue(msg, otid, dtid, type);

    return 0;
}
/* ------------------------------------------------ */
/* Initialize SIGTRAN / SCCP                        */
/* ------------------------------------------------ */

void sigtran_start() {
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

    sccp_user = osmo_sccp_user_bind(sccp, "tcap-router", sccp_prim_cb, 146);

    printf("SCCP stack initialized\n");
}

