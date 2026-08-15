#ifndef PERIPHERALS_H
#define PERIPHERALS_H

#include "sr_errno.h"

/**
 * @brief Brings up every peripheral this MCU/board needs before boot_core can run:
 * HAL, system clock, GPIO, CAN, RNG, etc.
 *
 * This is the one function a new MCU port must implement. Must be called once,
 * before any other mcu_interface function.
 *
 * @return sr_errno_t
 */
sr_errno_t sr_peripherals_init(void);

#endif
