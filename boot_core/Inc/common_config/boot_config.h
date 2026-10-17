#ifndef BOOT_CONFIG_H
#define BOOT_CONFIG_H

// IWDG timeout for a TEST boot. The app has to do handshake and confirm within this timeframe 
// Max on STM32 is ~32 s
#define CFG_TEST_BOOT_IWDG_TIMEOUT_MS 20000

#endif
