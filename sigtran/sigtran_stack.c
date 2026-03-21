#include "sigtran_stack.h"

#include <osmocom/core/msgb.h>
#include <osmocom/core/prim.h>
#include <osmocom/sigtran/osmo_ss7.h>
#include <osmocom/sigtran/osmo_ss7_as.h>
#include <osmocom/sigtran/osmo_ss7_asp.h>
#include <osmocom/sigtran/sccp_sap.h>

#include <stdint.h>
#include <stdio.h>

#include "../core/worker_pool.h"
#include "../router/tcap_parser.h"

#define DIR_FROM_STP 0

static struct osmo_ss7_instance *ss7;
static struct osmo_ss7_asp *asp;
static struct osmo_ss7_as *as;

struct osmo_sccp_instance *sccp;
struct osmo_sccp_user *sccp_user;

/* ========================================= */

static int sccp_prim_cb(struct osmo_prim_hdr *oph, void *ctx) {
  if (!oph || oph->primitive != OSMO_SCU_PRIM_N_DATA)
    return 0;

  struct msgb *msg = oph->msg;
  if (!msg)
    return 0;

  uint32_t otid = 0, dtid = 0;
  int type = 0;

  parse_tcap(msg->data, msg->len, &otid, &dtid, &type);

  msg->cb[0] = DIR_FROM_STP;

  worker_enqueue(msg, otid, dtid, type);

  return 0;
}

/* ========================================= */

void sigtran_start()
{
    printf("Initializing SIGTRAN → STP\n");

    osmo_ss7_init();

    ss7 = osmo_ss7_instance_find_or_create(NULL, 0);
    if (!ss7) {
        printf("SS7 instance failed\n");
        return;
    }

    osmo_ss7_instance_set_pc(ss7, 0x010101);

    /* ASP (M3UA client) */
    asp = osmo_ss7_asp_create(ss7, "tcap-asp", OSMO_SS7_ASP_PROT_M3UA);
    if (!asp) {
        printf("ASP create failed\n");
        return;
    }

    osmo_ss7_asp_set_peer(asp, "127.0.0.1", 2905);
    osmo_ss7_asp_set_rctx(asp, 0);
    osmo_ss7_asp_set_traffic_mode(asp, OSMO_SS7_TM_OVERRIDE);

    if (osmo_ss7_asp_start(asp) != 0) {
        printf("ASP start failed\n");
        return;
    }

    printf("M3UA ASP connected\n");

    /* SCCP */
    sccp = osmo_sccp_instance_create(ss7, NULL);
    if (!sccp) {
        printf("SCCP create failed\n");
        return;
    }

    sccp_user = osmo_sccp_user_bind(sccp, "tcap-router",
                                   sccp_prim_cb, 146);

    if (!sccp_user) {
        printf("SCCP bind failed\n");
        return;
    }

    printf("TCAP router ready (SSN 146)\n");
}
