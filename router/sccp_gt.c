#include "sccp_gt.h"
#include <string.h>

/*
 * NOTE: This is simplified:
 * assumes GT starts at fixed offset (lab/test env)
 */

int extract_calling_gt(uint8_t* sccp, int len, uint8_t* gt, int* gt_len)
{
    if (len < 20) return -1;

    int offset = 10; // adjust later

    int l = sccp[offset];

    if (l <= 0 || offset + l >= len) return -1;

    memcpy(gt, &sccp[offset+1], l);
    *gt_len = l;

    return 0;
}

void rewrite_calling_gt(uint8_t* sccp, int len,
                        uint8_t* new_gt, int new_len)
{
    if (len < 20) return;

    int offset = 10; // same assumption

    sccp[offset] = new_len;

    memcpy(&sccp[offset+1], new_gt, new_len);
}
