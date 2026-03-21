/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "53l8a1_ranging_sensor.h"
#include "stdio.h"
#include "NanoEdgeAI.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
  LOG,
} States_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define VERSION             150126u

#define TOF_SENSOR_ODR      15u
#define RESULT_VALUES_NB    1u
#define TOF_RESOLUTION      VL53L8CX_RESOLUTION_8X8
#define SIGNAL_SIZE         (TOF_RESOLUTION * RESULT_VALUES_NB)
#define DISTANCE_MAX        2000u
#define SKIPPED_RESULTS_NB  2u
#define LED_LOG_OK_PERIOD   75u
#define LED_LOG_NOK_PERIOD  25u

/* USER CODE END PD */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

RANGING_SENSOR_Capabilities_t Cap;
RANGING_SENSOR_ProfileConfig_t Profile;

__IO uint32_t ToF_EventFlag = SET;
__IO States_t appState = LOG;
__IO uint32_t ld2PeriodCnt = 0;
__IO uint32_t ld2Period = 0;
uint32_t resultCnt = 0;

float input_user_buffer[SIGNAL_SIZE] = {0};

float probabilities[NEAI_NUMBER_OF_CLASSES] = {0};
int   id_class = 0;
enum neai_state neai_state_var;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void ToF_Init(void);
void ToF_ProfileConfig(uint8_t resolution);
void ToF_Start(void);
int32_t FillBuffer(float *buffer);
void PrintBuffer(float *buffer);
void Log(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
  return len;
}

/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  MX_GPIO_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */

  printf("\r\nVer: %u.%u.%u\r\n", VERSION, SIGNAL_SIZE, TOF_SENSOR_ODR);

  ToF_Init();
  ToF_ProfileConfig(SIGNAL_SIZE);
  ToF_Start();

  neai_state_var = neai_classification_init();
  printf("NEAI init: %d\r\n", neai_state_var);

  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN 3 */
    switch (appState)
    {
      case LOG:
        Log();
        break;
      default:
        appState = LOG;
        break;
    }
    /* USER CODE END 3 */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    Error_Handler();

  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM            = 1;
  RCC_OscInitStruct.PLL.PLLN            = 10;
  RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    Error_Handler();

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
    Error_Handler();
}

/* USER CODE BEGIN 4 */

void ToF_Init(void)
{
  int32_t status = 0;

  printf("ToF sensor init...\r\n");

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);   HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET); HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);   HAL_Delay(2);

  status = VL53L8A1_RANGING_SENSOR_Init(VL53L8A1_DEV_CENTER);
  if (status != BSP_ERROR_NONE)
  {
    printf("ERROR\r\n");
    Error_Handler();
  }
  else
  {
    printf("OK\r\n");
  }
}

void ToF_ProfileConfig(uint8_t resolution)
{
  uint32_t Id;

  VL53L8A1_RANGING_SENSOR_ReadID(VL53L8A1_DEV_CENTER, &Id);
  VL53L8A1_RANGING_SENSOR_GetCapabilities(VL53L8A1_DEV_CENTER, &Cap);

  switch (resolution)
  {
    case VL53L8CX_RESOLUTION_8X8:
      Profile.RangingProfile = RS_PROFILE_8x8_CONTINUOUS;
      break;
    case VL53L8CX_RESOLUTION_4X4:
      Profile.RangingProfile = RS_PROFILE_4x4_CONTINUOUS;
      break;
    default:
      Profile.RangingProfile = RS_PROFILE_8x8_CONTINUOUS;
      break;
  }

  Profile.TimingBudget  = 30;
  Profile.Frequency     = TOF_SENSOR_ODR;
  Profile.EnableSignal  = 1;
  Profile.EnableAmbient = 0;

  VL53L8A1_RANGING_SENSOR_ConfigProfile(VL53L8A1_DEV_CENTER, &Profile);
  printf("ToF sensor ID: 0x%X\r\n", (int)Id);
}

void ToF_Start(void)
{
  int32_t status = VL53L8A1_RANGING_SENSOR_Start(VL53L8A1_DEV_CENTER, RS_MODE_BLOCKING_CONTINUOUS);
  if (status != BSP_ERROR_NONE)
  {
    printf("ERROR: TOF sensor start failed\r\n");
    Error_Handler();
  }
}

int32_t FillBuffer(float *buffer)
{
  RANGING_SENSOR_Result_t Result;
  int32_t status = 0;

  if (ToF_EventFlag)
  {
    ToF_EventFlag = RESET;
    status = VL53L8A1_RANGING_SENSOR_GetDistance(VL53L8A1_DEV_CENTER, &Result);
    if (status == BSP_ERROR_NONE)
    {
      resultCnt++;
      for (uint16_t i = 0; i < TOF_RESOLUTION; i++)
      {
        uint32_t dist = Result.ZoneResult[i].Distance[0];
        buffer[i] = (dist > 0 && dist <= DISTANCE_MAX) ? (float)dist : (float)DISTANCE_MAX;
      }
    }
  }
  return status;
}

void PrintBuffer(float *buffer)
{
  for (uint16_t i = 0; i < SIGNAL_SIZE; i++)
  {
    if (i < SIGNAL_SIZE - 1)
      printf("%d,", (int)buffer[i]);
    else
      printf("%d\n", (int)buffer[i]);
  }
}

void Log(void)
{
  int32_t status = FillBuffer(input_user_buffer);

  if (resultCnt > SKIPPED_RESULTS_NB)
  {
    if (status == 0)
    {
      ld2Period = LED_LOG_OK_PERIOD;

      PrintBuffer(input_user_buffer);

      neai_classification(input_user_buffer, probabilities, &id_class);

      printf("# CLASS:%d CONF:%.0f%%\r\n",
             id_class,
             probabilities[id_class] * 100.0f);

      printf("# -> %s\r\n", neai_get_class_name(id_class));
    }
    else
    {
      ld2Period = LED_LOG_NOK_PERIOD;
    }
  }
  else
  {
    printf("Waiting for valid data...\r\n");
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_4)
    ToF_EventFlag = SET;
}

void HAL_IncTick(void)
{
  uwTick += (uint32_t)uwTickFreq;
  if (ld2PeriodCnt > 0)
  {
    ld2PeriodCnt--;
  }
  else
  {
    ld2PeriodCnt = ld2Period;
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
  }
}

/* USER CODE END 4 */

void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1) {}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
