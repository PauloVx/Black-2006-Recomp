#ifndef BLACK_LOCALIZATION_H
#define BLACK_LOCALIZATION_H

#include <stdint.h>

/* IDs computed with BLACK's CRC table for the frontend tokens in the shipped XBE. */
static inline uint32_t black_localization_fallback_record(uint32_t query_hash)
{
    switch (query_hash) {
    case 0x3A7C7126u: /* FE_LEGALSCREENTEXT */
        return 0x3A7C7126u;
    case 0x46C7A416u: /* FE__LOADING; the bank uses the FE_LOADING record */
    case 0x545DE522u: /* FE_LOADING */
        return 0x545DE522u;
    default:
        return 0u;
    }
}

#endif
