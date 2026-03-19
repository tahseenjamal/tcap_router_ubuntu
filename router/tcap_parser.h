#ifndef TCAP_PARSER_H
#define TCAP_PARSER_H

#include <stdint.h>

void parse_tcap(uint8_t *d, int len, uint32_t *otid, uint32_t *dtid, int *type);

#endif
