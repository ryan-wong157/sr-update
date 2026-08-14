#ifndef SRV_SESSION_CONTROL_H
#define SRV_SESSION_CONTROL_H

#include <stdint.h>
#include "sr_errno.h"

typedef enum {
    SESSION_DEFAULT = 0,
    SESSION_PROGRAMMING
} session_state_t;

void set_session(session_state_t sesn);
session_state_t get_session(void);

// Reset S3 Server
void s3_refresh(void);

// Check if s3 timeout has expired, if it has, it auto-resets session and security and prog
void s3_check_timeout(void);

sr_errno_t x10_sess_ctrl_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

#endif
