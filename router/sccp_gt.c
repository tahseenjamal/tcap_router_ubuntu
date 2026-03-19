#include "sccp_gt.h"

#include <string.h>

/*
 * SCCP Address Indicator bits (per ITU-T Q.713)
 * bit0: routing indicator (0=route on GT, 1=route on SSN/PC)
 * bit1: global title indicator present
 * bit2: SSN present
 * bit3: PC present
 */

static int parse_address(uint8_t *buf, int len, uint8_t *gt, int *gt_len,
                         int rewrite, uint8_t *new_gt, int new_len) {
    if (len < 1) return -1;

    int off = 0;
    uint8_t ai = buf[off++];

    int has_gt = (ai & 0x02) != 0;
    int has_ssn = (ai & 0x04) != 0;
    int has_pc = (ai & 0x08) != 0;

    /* skip PC (2 bytes typically) */
    if (has_pc) {
        if (off + 2 > len) return -1;
        off += 2;
    }

    /* skip SSN (1 byte) */
    if (has_ssn) {
        if (off + 1 > len) return -1;
        off += 1;
    }

    if (!has_gt) return -1;

    /* GT format: first byte is GT indicator/format */
    if (off + 2 > len) return -1;

    uint8_t gt_indicator = buf[off++];

    /* next byte is length of GT digits */
    int l = buf[off++];

    if (l <= 0 || off + l > len) return -1;

    if (!rewrite) {
        memcpy(gt, &buf[off], l);
        *gt_len = l;
    } else {
        /* rewrite length + digits */
        buf[off - 1] = new_len; /* length byte */
        memcpy(&buf[off], new_gt, new_len);
    }

    return 0;
}

/*
 * Extract Calling Party Address GT
 * NOTE: assumes SCCP header already points to address section
 */
int extract_calling_gt(uint8_t *sccp, int len, uint8_t *gt, int *gt_len) {
    if (len < 5) return -1;

    /* UDT layout:
     * [msg type][called addr ptr][calling addr ptr][data ptr]...
     */
    uint8_t calling_ptr = sccp[2];

    if (calling_ptr == 0 || calling_ptr >= len) return -1;

    return parse_address(&sccp[calling_ptr], len - calling_ptr, gt, gt_len, 0,
                         NULL, 0);
}

void rewrite_calling_gt(uint8_t *sccp, int len, uint8_t *new_gt, int new_len) {
    if (len < 5) return;

    uint8_t calling_ptr = sccp[2];

    if (calling_ptr == 0 || calling_ptr >= len) return;

    parse_address(&sccp[calling_ptr], len - calling_ptr, NULL, NULL, 1, new_gt,
                  new_len);
}
