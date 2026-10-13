#ifndef UDS_CONFIG_H
#define UDS_CONFIG_H

#include <stdint.h>

// How many bytes should 0x36 transfer data send (including 2 non-data bytes)
#define CFG_UDS_x36_MAX_BLOCK_LEN 258

// Buffer sizes
#define CFG_UDS_TX_BUF_SIZE 256
#define CFG_UDS_RX_BUF_SIZE (CFG_UDS_x36_MAX_BLOCK_LEN * 2) // arbitrary

// Timing params all in ms (star are in 10s of ms)
#define CFG_S3_SERVER 5000
#define CFG_DEFAULT_P2_SERVER_MAX 50
#define CFG_DEFAULT_P2STAR_SERVER_MAX (5000 / 10)
#define CFG_PROG_P2_SERVER_MAX 50
#define CFG_PROG_P2STAR_SERVER_MAX (20000 / 10)

#define CFG_MAX_x27_ATTEMPTS 3
#define CFG_RETRY_TIMEOUT 60000 // 1min
#endif