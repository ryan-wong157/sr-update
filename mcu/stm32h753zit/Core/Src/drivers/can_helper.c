#include "can_helper.h"

static const uint32_t dlc_to_bytes_table[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
};

uint32_t dlc_to_bytes(uint32_t dlc) {
    if (dlc >= sizeof(dlc_to_bytes_table) / sizeof(dlc_to_bytes_table[0])) {
        return 0;
    }
    return dlc_to_bytes_table[dlc];
}

uint32_t bytes_to_dlc(uint32_t length) {
    if (length > 64) {
        return UINT32_MAX;
    }
    for (uint32_t dlc = 0; dlc < sizeof(dlc_to_bytes_table) / sizeof(dlc_to_bytes_table[0]); dlc++) {
        if (dlc_to_bytes_table[dlc] >= length) {
            return dlc;
        }
    }
    return UINT32_MAX;
}