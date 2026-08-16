// Brings up every peripheral this board needs, mirroring what a CubeMX-generated
// main() would normally do. Core/Src/main.c is left untouched and unused so that
// regenerating it from the .ioc never requires hand-editing generated code back in.

#include "main.h"
#include "gpio.h"
#include "fdcan.h"
#include "rng.h"
#include "rtc.h"
#include "mcu_interface/peripherals.h"

// Defined in the CubeMX-generated Core/Src/main.c but not declared in main.h
void SystemClock_Config(void);

sr_errno_t sr_peripherals_init(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_FDCAN1_Init();
    MX_RNG_Init();
    MX_RTC_Init();
    return SR_OK;
}
