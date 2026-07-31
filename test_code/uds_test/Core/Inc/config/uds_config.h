#ifndef UDS_CONFIG_H
#define UDS_CONFIG_H

#include "config/flash_config.h"

// Buffer sizes
#define CFG_UDS_TX_BUF_SIZE 256
#define CFG_UDS_RX_BUF_SIZE 258 // SID, sequence counter, 256 transfer bytes for 0x36 

// Timing params all in ms (star are in 10s of ms)
#define CFG_S3_SERVER 5000
#define CFG_DEFAULT_P2_SERVER_MAX 50
#define CFG_DEFAULT_P2STAR_SERVER_MAX (5000 / 10)
#define CFG_PROG_P2_SERVER_MAX 50
#define CFG_PROG_P2STAR_SERVER_MAX (20000 / 10)

// 0x27 32 bit public key
static const uint8_t x27_PUB_KEY[32] = {0x5d, 0x01, 0x64, 0x00, 0x87, 0x7a, 0x12, 0x0e, 0xd6, 0xce, 0x63, 0x32, 0x4b, 0x42, 0x6b, 0x0f, 0x56, 0xff, 0x40, 0x78, 0x7d, 0x15, 0x2f, 0x20, 0x06, 0x84, 0x59, 0xe8, 0xde, 0x24, 0x6d, 0x31};
#define CFG_MAX_x27_ATTEMPTS 3
#define CFG_RETRY_TIMEOUT 60000 // 1min
#endif