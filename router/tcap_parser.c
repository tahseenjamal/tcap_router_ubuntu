#include "tcap_parser.h"

static int read_len(uint8_t *d, int *len_bytes) {
    if (d[0] < 0x80) {
        *len_bytes = 1;
        return d[0];
    }

    int n = d[0] & 0x7F;
    int val = 0;

    for (int i = 0; i < n; i++) val = (val << 8) | d[1 + i];

    *len_bytes = 1 + n;
    return val;
}

void parse_tcap(uint8_t *d, int len, uint32_t *otid, uint32_t *dtid,
                int *type) {
    *otid = 0;
    *dtid = 0;
    *type = 0;

    if (len < 2) return;

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
        case 0x67:
            *type = 4;
            break;
        default:
            return;
    }

    int i = 1;

    while (i < len) {
        uint8_t tag = d[i++];

        int lbytes = 0;
        int l = read_len(&d[i], &lbytes);
        i += lbytes;

        if (i + l > len) break;

        if (tag == 0x48) {
            uint32_t id = 0;
            for (int j = 0; j < l; j++) id = (id << 8) | d[i + j];
            *otid = id;
        }

        if (tag == 0x49) {
            uint32_t id = 0;
            for (int j = 0; j < l; j++) id = (id << 8) | d[i + j];
            *dtid = id;
        }

        i += l;
    }
}
