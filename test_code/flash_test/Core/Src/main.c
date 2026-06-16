/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fdcan.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "drivers/flash_driver.h"
#include "drivers/can_driver.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// RX IDS
#define FUNNY_MSG_ID 0x05
#define ANGRY_MSG_ID 0x07
#define SAD_MSG_ID 0x0A
#define ODD_MSG_ID 0x99

// TX IDS
#define FUNNY_RESPONSE_ID 0x06
#define ANGRY_RESPONSE_ID 0x08
#define SAD_RESPONSE_ID 0x0B
#define ODD_RESPONSE_ID 0x9A


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
typedef struct {
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t data[8];
    volatile uint8_t new_data;
} rx_can_msg_t;

static rx_can_msg_t funny_msg;
static rx_can_msg_t angry_msg;
static rx_can_msg_t sad_msg;
static rx_can_msg_t odd_msg;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  /* USER CODE BEGIN 2 */
  sr_fdcan_config_t cfg = {
    .fifo_overwrite=FDCAN_RX_FIFO_BLOCKING,
    .tx_id_type=FDCAN_STANDARD_ID,
    .tx_brs=FDCAN_FRAME_FD_NO_BRS,
    .tx_frame_format=FDCAN_CLASSIC_CAN,
    .tx_event_fifo_control=FDCAN_NO_TX_EVENTS
  };

  sr_fdcan_config(&hfdcan1, &cfg);
  sr_fdcan_filter_add(&hfdcan1, FDCAN_FILTER_RANGE, FDCAN_FILTER_TO_RXFIFO0, 0x05, 0x0A);
  sr_fdcan_filter_add(&hfdcan1, FDCAN_FILTER_DUAL, FDCAN_FILTER_TO_RXFIFO0, 0x99, 0x100);

  HAL_FDCAN_Start(&hfdcan1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
    /* USER CODE END WHILE */
    
    /* USER CODE BEGIN 3 */
        char data[8];
        // Check if new data in any of the rx message structs
        if (funny_msg.new_data) {
            snprintf(data, sizeof(data), "funny!");
            sr_fdcan_tx(&hfdcan1, FUNNY_RESPONSE_ID, (uint8_t*)data, FDCAN_DLC_BYTES_7);
            funny_msg.new_data = 0;
        } else if (angry_msg.new_data) {
            snprintf(data, sizeof(data), "angry!");
            sr_fdcan_tx(&hfdcan1, ANGRY_RESPONSE_ID, (uint8_t*)data, FDCAN_DLC_BYTES_7);
            angry_msg.new_data = 0;        
        } else if (sad_msg.new_data) {
            snprintf(data, sizeof(data), "sad!");
            sr_fdcan_tx(&hfdcan1, SAD_RESPONSE_ID, (uint8_t*)data, FDCAN_DLC_BYTES_5);
            sad_msg.new_data = 0;
        } else if (odd_msg.new_data) {
            snprintf(data, sizeof(data), "WRITING");
            sr_fdcan_tx(&hfdcan1, FUNNY_RESPONSE_ID, (uint8_t*)data, FDCAN_DLC_BYTES_8);
            odd_msg.new_data = 0;

            uint32_t write_addr = 0x0801F800;
            sr_flash_erase_page(FLASH_BANK_1, 63, 1);
            uint64_t val;
            memcpy(&val, odd_msg.data, 8);
            sr_flash_write64(write_addr, val);
            if (*(volatile uint64_t*)write_addr == val) {
                snprintf(data, sizeof(data), "SUCCESS");
                sr_fdcan_tx(&hfdcan1, FUNNY_RESPONSE_ID, (uint8_t*)data, FDCAN_DLC_BYTES_8);
            }

        }
        // Do nothing
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_FDCAN_RxFifo0Callback (FDCAN_HandleTypeDef * hfdcan, uint32_t RxFifo0ITs) {
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
        FDCAN_RxHeaderTypeDef head;
        uint8_t data[8];
        HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &head, data);

        // now dispatch
        switch(head.Identifier) {
            case FUNNY_MSG_ID: 
                funny_msg.rx_header = head;
                memcpy(funny_msg.data, data, 8);
                funny_msg.new_data = 1;
                break;
            case ANGRY_MSG_ID: 
                angry_msg.rx_header = head;
                memcpy(angry_msg.data, data, 8);
                angry_msg.new_data = 1;
                break;
            case SAD_MSG_ID: 
                sad_msg.rx_header = head;
                memcpy(sad_msg.data, data, 8);
                sad_msg.new_data = 1;
                break;
            case ODD_MSG_ID: 
                odd_msg.rx_header = head;
                memcpy(odd_msg.data, data, 8);
                odd_msg.new_data = 1;
                break;
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state
     */
    __disable_irq();
    while (1) {
    }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
