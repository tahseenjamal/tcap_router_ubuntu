#ifndef SCCP_GT_H
#define SCCP_GT_H

#include <stdint.h>

int extract_calling_gt(uint8_t* sccp, int len, uint8_t* gt, int* gt_len);

int rewrite_calling_gt(uint8_t* sccp, int len,
                       uint8_t* new_gt, int new_len);

#endif
