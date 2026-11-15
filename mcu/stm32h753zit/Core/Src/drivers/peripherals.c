// Brings up every peripheral this board needs, mirroring what a CubeMX-generated
// main() would normally do. Core/Src/main.c is left untouched and unused so that
// regenerating it from the .ioc never requires hand-editing generated code back in.

#include "main.h"
#include "gpio.h"
#include "fdcan.h"
#include "rng.h"
#include "rtc.h"
#include "tim.h"
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
    // TEMP benchmark tick source (drivers/bench_timer.c)
    MX_TIM2_Init();
    HAL_TIM_Base_Start(&htim2);
    return SR_OK;
}

sr_errno_t sr_peripherals_deinit(void) {
    HAL_FDCAN_Stop(&hfdcan1);
    HAL_FDCAN_DeInit(&hfdcan1);
    HAL_RNG_DeInit(&hrng);
    HAL_RTC_DeInit(&hrtc);
    HAL_TIM_Base_Stop(&htim2);
    HAL_TIM_Base_DeInit(&htim2);
    HAL_RCC_DeInit();
    HAL_DeInit();
    return SR_OK;
}
