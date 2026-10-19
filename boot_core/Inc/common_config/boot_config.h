#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

// IWDG timeout for a TEST boot. The app has to do handshake and confirm within this timeframe 
// Max on STM32 is ~32 s
#define CFG_TEST_BOOT_IWDG_TIMEOUT_MS 20000

#define CFG_HANDSHAKE_TX_ID (0x1F000000u | ((uint32_t)(ECU_ID) << 12) | 0xFFDu)
#define HANDSHAKE_STATUS_REJECTED_INVALID 0x03u
#define HANDSHAKE_STATUS_REJECTED_OLDER 0x04u
#define CFG_REJECTION_REPEATS 3

#endif
