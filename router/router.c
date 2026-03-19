#include "router.h"
#include "sccp_gt.h"

#include <osmocom/core/msgb.h>
#include <stdatomic.h>
#include <stdint.h>
#include <sys/socket.h>

#include "../core/backend_pool.h"
#include "../core/transaction_table.h"

/*
 * Extract SCCP user data (TCAP payload)
 */

static uint8_t SELF_GT[] = {0x91, 0x88, 0x77, 0x66};
static int SELF_GT_LEN = 4;

static uint8_t *extract_sccp_userdata(struct msgb *msg, int *len) {
  uint8_t *d = msg->data;

  if (msg->len < 5)
    return NULL;

  uint8_t msg_type = d[0];

  /* Handle UDT / XUDT / LUDT */

  if (msg_type != 0x09 && msg_type != 0x11 && msg_type != 0x13)
    return NULL;

  uint8_t ptr_data = d[3];

  /* Validate SCCP pointer */

  if (ptr_data == 0 || ptr_data >= msg->len)
    return NULL;

  int data_offset = ptr_data + 1;

  if (data_offset >= msg->len)
    return NULL;

  *len = msg->len - data_offset;

  return &d[data_offset];
}

/*
 * TCAP routing logic
 */

void route_tcap(struct msgb *msg, uint32_t otid, uint32_t dtid, int type) {
  backend_t *b = NULL;

  int tcap_len = 0;

  uint8_t *tcap = extract_sccp_userdata(msg, &tcap_len);

  if (!tcap) {
    msgb_free(msg);
    return;
  }

  /* BEGIN detection */
  int is_begin = (type == 1 && otid != 0);

  if (is_begin) {

    /* New dialog */

    b = backend_choose();

    if (!b || !otid) {
      msgb_free(msg);
      return;
    }

    uint8_t orig_gt[32];
    int gt_len = 0;

    if (extract_calling_gt(msg->data, msg->len, orig_gt, &gt_len) < 0) {
      gt_len = 0;
    }

    /* rewrite GT */
    if (info.gt_len > 0) {
      rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);
    }

    /* store full mapping */
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

    /* restore GT */
    rewrite_calling_gt(msg->data, msg->len, info.gt, info.gt_len);

    /* END cleanup — DTID is correct key */

    if (type == 3 && dtid) {
      tx_delete(dtid);
    }
  }

  /* Send to backend */

  atomic_fetch_add(&b->load, 1);

  ssize_t sent = send(b->sock, msg->data, msg->len, MSG_NOSIGNAL);

  atomic_fetch_sub(&b->load, 1);

  if (sent <= 0) {
    atomic_store(&b->active, 0);
  }

  msgb_free(msg);
}
