#include <stdint.h>
#include "main.h"
#include "rng.h"
#include "mcu_interface/rng_driver.h"

static RNG_HandleTypeDef* my_hrng = &hrng;

sr_errno_t sr_rng_start() {
    my_hrng = hrng;
    if (HAL_RNG_Init(hrng) != HAL_OK) {
        return ERR_RNG_FAIL_START;
    }
    return SR_OK;
}

sr_errno_t sr_generate_number(uint32_t* rn) {
    if (HAL_RNG_GenerateRandomNumber(my_hrng, rn) == HAL_OK) {
        return SR_OK;
    }

    // If fail, generate pseudo-random. According to 
    // https://community.st.com/stm32-mcus-products-25/stm32h7b3i-random-number-generator-occasional-seed-error-11899
    // Seed fails or clock fails are not extremely infrequent.
    // Use https://en.wikipedia.org/wiki/Xorshift (cuz simple)
    static uint32_t seed = 0;
    if (seed == 0) {
        seed = HAL_GetTick() ^ 0x1254AB38;
        if (seed == 0) seed = 0x1254AB38;
    }
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    *rn = seed;

    return SR_OK;
}