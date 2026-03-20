#include "tcap_parser.h"

#include <stdint.h>
#include <stdio.h>

/* =========================================================
 * Safe length reader (ASN.1 style)
 * ========================================================= */
static int read_len(uint8_t *d, int max, int *len_bytes) {
    if (!d || max <= 0) return -1;

    /* short form */
    if (d[0] < 0x80) {
        *len_bytes = 1;
        return d[0];
    }

    /* long form */
    int n = d[0] & 0x7F;

    /* sanity checks */
    if (n <= 0 || n > 4 || (1 + n) > max) return -1;

    int val = 0;

    for (int i = 0; i < n; i++) {
        val = (val << 8) | d[1 + i];
    }

    *len_bytes = 1 + n;
    return val;
}

/* =========================================================
 * TCAP parser (defensive)
 * ========================================================= */
void parse_tcap(uint8_t *d, int len, uint32_t *otid, uint32_t *dtid,
                int *type) {
    *otid = 0;
    *dtid = 0;
    *type = 0;

    if (!d || len < 2) return;

    /* TCAP message type */
    switch (d[0]) {
        case 0x62:
            *type = 1;
            break; /* BEGIN */
        case 0x65:
            *type = 2;
            break; /* ABORT */
        case 0x64:
            *type = 3;
            break; /* END */
        case 0x67:
            *type = 4;
            break; /* CONTINUE */
        default:
            return; /* unknown */
    }

    int i = 1;

    /* limit scan to avoid malformed packets causing issues */
    int max_scan = (len < 512) ? len : 512;

    while (i < max_scan) {
        if (i >= len) break;

        uint8_t tag = d[i++];

        if (i >= len) break;

        int lbytes = 0;
        int l = read_len(&d[i], len - i, &lbytes);

        if (l < 0) break;

        i += lbytes;

        if (i + l > len) break;

        /* OTID */
        if (tag == 0x48 && l > 0 && l <= 4) {
            uint32_t id = 0;
            for (int j = 0; j < l; j++) id = (id << 8) | d[i + j];

            *otid = id;
        }

        /* DTID */
        if (tag == 0x49 && l > 0 && l <= 4) {
            uint32_t id = 0;
            for (int j = 0; j < l; j++) id = (id << 8) | d[i + j];

            *dtid = id;
        }

        /* move to next TLV */
        i += l;
    }

    /* =====================================================
     * Validation (important for routing safety)
     * ===================================================== */

    if (*type == 1) { /* BEGIN */
        if (*otid == 0) {
            printf("PARSE WARN: BEGIN without OTID\n");
        }
    } else {
        if (*dtid == 0) {
            printf("PARSE WARN: non-BEGIN without DTID (type=%d)\n", *type);
        }
    }
}
