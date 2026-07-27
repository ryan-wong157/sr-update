#ifndef UDS_CONFIG_H
#define UDS_CONFIG_H

// Buffer sizes
#define CFG_UDS_TX_BUF_SIZE 16
#define CFG_UDS_RX_BUF_SIZE 2048

// Timing params all in ms (star are in 10s of ms)
#define S3_SERVER 5000
#define DEFAULT_P2_SERVER_MAX 50
#define DEFAULT_P2STAR_SERVER_MAX (5000 / 10)
#define PROG_P2_SERVER_MAX 50
#define PROG_P2STAR_SERVER_MAX (20000 / 10)

#endif