#include "sccp_gt.h"

#include <string.h>

static int parse_address(uint8_t *buf, int len, uint8_t *gt, int *gt_len,
                         int rewrite, uint8_t *new_gt, int new_len) {
    if (len < 1) return -1;

    int off = 0;
    uint8_t ai = buf[off++];

    int has_gt = (ai & 0x02);
    int has_ssn = (ai & 0x04);
    int has_pc = (ai & 0x08);

    if (has_pc) {
        if (off + 2 > len) return -1;
        off += 2;
    }

    if (has_ssn) {
        if (off + 1 > len) return -1;
        off += 1;
    }

    if (!has_gt) return -1;

    if (off + 2 > len) return -1;

    off++;  // GT indicator
    int l = buf[off++];

    if (l <= 0 || off + l > len) return -1;

    if (!rewrite) {
        memcpy(gt, &buf[off], l);
        *gt_len = l;
    } else {
        if (new_len > l) return -1;  // ✅ critical fix
        buf[off - 1] = new_len;
        memcpy(&buf[off], new_gt, new_len);
    }

    return 0;
}

int extract_calling_gt(uint8_t *sccp, int len, uint8_t *gt, int *gt_len) {
    if (len < 5) return -1;

    uint8_t ptr = sccp[2];

    if (ptr == 0 || ptr >= len) return -1;

    return parse_address(&sccp[ptr], len - ptr, gt, gt_len, 0, NULL, 0);
}

void rewrite_calling_gt(uint8_t *sccp, int len, uint8_t *new_gt, int new_len) {
    if (len < 5) return;

    uint8_t ptr = sccp[2];

    if (ptr == 0 || ptr >= len) return;

    parse_address(&sccp[ptr], len - ptr, NULL, NULL, 1, new_gt, new_len);
}
