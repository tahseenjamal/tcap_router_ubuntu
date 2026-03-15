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

  /*
   * Dialog routing
   *
   * BEGIN      -> choose backend
   * CONTINUE   -> lookup DTID
   * END        -> lookup DTID then remove
   */

  if (type == 1) {

    /* BEGIN */

    b = backend_choose();

    if (b && otid)
      tx_store(otid, b->id);

  } else {

    /* CONTINUE / END */

    int backend = tx_lookup(dtid);

    b = backend_get(backend);

    /* END -> remove dialog */

    if (type == 3 && dtid)
      tx_delete(dtid);
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
    atomic_store(&b->active, 0);
  }

  msgb_free(msg);
}
