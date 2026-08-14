#ifndef SRV_SECURITY_ACCESS_H
#define SRV_SECURITY_ACCESS_H

#include <stdint.h>
#include "sr_errno.h"

typedef enum {
    SECURITY_LOCKED = 0,
    SECURITY_UNLOCKED
} security_access_t;

typedef enum {
    EXPECTING_REQUEST = 0,
    EXPECTING_SIGNATURE
} x27_state_t;

security_access_t get_security_access(void);

// Should be called when changing session. re-locks security and clears any
// in-progress seed/key exchange.
void x27_on_session_change(void);

sr_errno_t x27_sec_access_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

#endif
