#ifndef RNG_DRIVER_H
#define RNG_DRIVER_H

#include "main.h"
#include "sr_errno.h"

// legit completely unecessary but isolates the user of rng from the HAL

/**
 * @brief Starts rng peripheral
 * 
 * @return sr_errno_t 
 */
sr_errno_t sr_rng_start();

/**
 * @brief Generates 32 bit random number. If peripheral fails, generate pseudo random number
 * 
 * @param hrng 
 * @param rn - pointer to a 32 bit int
 * @return sr_errno_t 
 */
sr_errno_t sr_generate_number(uint32_t* rn);

#endif