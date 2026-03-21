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
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_tim.h"
#include "53l8a1_ranging_sensor.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
  LOG,
} States_t;

typedef struct {
    uint16_t pitch_arr;      // Częstotliwość dźwięku (wysokość tonu)
    uint16_t beep_interval;  // Przerwa między piknięciami (odległość)
    uint32_t last_tick;      // Czas ostatniej zmiany stanu
    uint8_t  is_on;          // Czy aktualnie wydaje dźwięk
    uint8_t  active;         // Czy buzzer w ogóle ma pracować
} Buzzer_t;

// Globalne stany buzzerów
Buzzer_t bz_L = {999, 500, 0, 0, 0}; // Lewy
Buzzer_t bz_R = {999, 500, 0, 0, 0}; // Prawy

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define VERSION        150126u

#define TOF_SENSOR_ODR      3u /* Output Data Rate [Hz] */
#define RESULT_VALUES_NB    2u /* Logged result values: DISTANCE, SIGNAL */
#define TOF_RESOLUTION      VL53L8CX_RESOLUTION_8X8
#define SIGNAL_SIZE         (TOF_RESOLUTION * RESULT_VALUES_NB)
#define DISTANCE_MAX        2000u  /* [mm] */
#define SKIPPED_RESULTS_NB  2u  /* First ToF sensor results to be skipped: transition stage */
#define LED_LOG_OK_PERIOD   75u  /* [ms] */
#define LED_LOG_NOK_PERIOD  25u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
TIM_HandleTypeDef htim2;
RANGING_SENSOR_Capabilities_t Cap;
RANGING_SENSOR_ProfileConfig_t Profile;

__IO uint32_t ToF_EventFlag = SET;
__IO States_t appState = LOG;
__IO uint32_t ld2PeriodCnt = 0;  /* LD2 toggling counter incremented by SysTick */
__IO uint32_t ld2Period = 0;  /* LD2 toggle timing to be set by user */
uint32_t resultCnt = 0;
float input_user_buffer[SIGNAL_SIZE] = {0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void MX_TIM2_Init(void);
void ToF_Init(void);
void ToF_ProfileConfig(uint8_t resolution);
void ToF_Start(void);
int32_t FillBuffer(float *buffer);
void PrintBuffer(float *buffer);
void Log(void);
void Buzzer_Handler(Buzzer_t *bz, TIM_HandleTypeDef *htim, uint32_t channel);
void Set_Buzzer(Buzzer_t *bz, uint8_t row, uint16_t distance);  // FIX: dodano brakujący prototyp

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int _write(int file, char *ptr, int len)
{
  HAL_UART_Transmit(&huart2,(uint8_t*)ptr,len,100);
  return len;
}

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
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  MX_TIM2_Init();  // FIX: PWM NIE jest startowany wewnątrz MX_TIM2_Init – tylko konfiguracja

  printf("\r\nVer: %u.%u.%u\r\n", VERSION, SIGNAL_SIZE, TOF_SENSOR_ODR);

  // TRYB TESTOWY: stały sygnał PWM 1000Hz, 50% wypełnienia na obu kanałach
  // Multimetr powinien pokazać ~1.65V na PA0 i PA1
  // Po testach – zakomentuj te 6 linii
  __HAL_TIM_SET_AUTORELOAD(&htim2, 999);        // ARR=999 → 1MHz/1000 = 1000Hz
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 499); // 50% wypełnienia CH1
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 499); // 50% wypełnienia CH2
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
  printf("TEST: PWM 1000Hz 50%% wystartowal na PA0 i PA1\r\n");

  ToF_Init();
  ToF_ProfileConfig(SIGNAL_SIZE);
  ToF_Start();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    switch (appState)
    {
      case LOG:
        Log();
        break;
      default:
        appState = LOG;
        break;
    }

    // FIX: Odkomentowano – bez tego buzzery nigdy nie będą obsługiwane
    Buzzer_Handler(&bz_L, &htim2, TIM_CHANNEL_1);
    Buzzer_Handler(&bz_R, &htim2, TIM_CHANNEL_2);
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

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  __HAL_RCC_TIM2_CLK_ENABLE();  // FIX: Bez tego timer stoi w miejscu – zegar TIM2 musi być włączony przed inicjalizacją

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 79;           // 80MHz / (79+1) = 1MHz takt licznika
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;             // 1MHz / (999+1) = 1000Hz domyślnie
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

  // FIX: Używamy HAL_TIM_PWM_Init zamiast HAL_TIM_Base_Init – tylko inicjalizacja, bez startu
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 500;               // 50% wypełnienia
  sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  // FIX: HAL_TIM_PWM_Start USUNIĘTY z tej funkcji – start jest w main()
  // Fizyczne połączenie Timera z pinami procesora
  HAL_TIM_MspPostInit(&htim2);
}

// Konfiguracja pinów PA0 i PA1 jako wyjścia PWM (TIM2 CH1 i CH2)
void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if (htim->Instance == TIM2)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /*
     * TIM2 GPIO Configuration:
     * PA0  ------> TIM2_CH1  (Lewy buzzer)
     * PA1  ------> TIM2_CH2  (Prawy buzzer)
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

/* USER CODE END 4 */

void ToF_Init(void)
{
  int32_t status = 0;

  printf("ToF sensor init...\r\n");

  /* Sensor reset */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(2);

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

  Profile.TimingBudget = 30; /* 5 ms < TimingBudget < 100 ms */
  Profile.Frequency = TOF_SENSOR_ODR; /* Hz */
  Profile.EnableSignal = 1;
  Profile.EnableAmbient = 0;

  VL53L8A1_RANGING_SENSOR_ConfigProfile(VL53L8A1_DEV_CENTER, &Profile);
  printf("ToF sensor ID: 0x%X\r\n", (int)Id);
}

void ToF_Start(void)
{
  int32_t status = 0;

  status = VL53L8A1_RANGING_SENSOR_Start(VL53L8A1_DEV_CENTER, RS_MODE_BLOCKING_CONTINUOUS);
  if (status != BSP_ERROR_NONE)
  {
    printf("ERROR: TOF sensor start failed\r\n");
    Error_Handler();
  }
}

int32_t FillBuffer(float *buffer)
{
  uint16_t i, j;
  RANGING_SENSOR_Result_t Result;
  int32_t status = 0;

  if (ToF_EventFlag)
  {
    ToF_EventFlag = RESET;
    status = VL53L8A1_RANGING_SENSOR_GetDistance(VL53L8A1_DEV_CENTER, &Result);
    if (status == BSP_ERROR_NONE)
    {
      resultCnt++;
      j = 0;
      for (i = 0; i < TOF_RESOLUTION; i++)
      {
        if (Result.ZoneResult[i].Distance[0] <= DISTANCE_MAX)
        {
          buffer[j++] = (float)Result.ZoneResult[i].Distance[0];
        }
        else
        {
          buffer[j++] = DISTANCE_MAX;
        }
        buffer[j++] = (float)Result.ZoneResult[i].Signal[0];
      }
    }
  }
  return status;
}

void PrintBuffer(float *buffer)
{
  uint16_t i = 0;

  while (i < SIGNAL_SIZE)
  {
    if (i < SIGNAL_SIZE - 1)
    {
      printf("%d ", (int)buffer[i]);
    }
    else
    {
      printf("%d\n", (int)buffer[i]);
    }
    i++;
  }
}

void Log(void)
{
  int32_t status = 0;
  status = FillBuffer(input_user_buffer);

  if (resultCnt > SKIPPED_RESULTS_NB)
  {
    if (status == 0)
    {
      ld2Period = LED_LOG_OK_PERIOD;

      /*
       * Bufor ma strukturę: [Dist_0, Sig_0, Dist_1, Sig_1, ..., Dist_63, Sig_63]
       * Środek mapy 8x8 to strefy 27 i 36 (wiersze 3 i 4, kolumny 3 i 4).
       * FIX: Używamy strefy 27 jako reprezentatywnego centrum.
       * Indeks w buforze: strefa_nr * 2 = indeks dystansu
       */
      uint16_t center_dist = (uint16_t)input_user_buffer[27 * 2]; // Strefa 27 = środek mapy

      if (center_dist < 1500)
      {
        // FIX: Ustawiamy oba buzzery na podstawie dystansu – lewy i prawy
        Set_Buzzer(&bz_L, 1, center_dist);  // Środkowa strefa → ton średni
        Set_Buzzer(&bz_R, 1, center_dist);  // Prawy buzzer tak samo
      }
      else
      {
        bz_L.active = 0;  // Cisza gdy pusto
        bz_R.active = 0;
      }

      PrintBuffer(input_user_buffer);
    }
    else
    {
      ld2Period = LED_LOG_NOK_PERIOD;
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_4)
  {
    ToF_EventFlag = SET;
  }
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

void Buzzer_Handler(Buzzer_t *bz, TIM_HandleTypeDef *htim, uint32_t channel)
{
  if (!bz->active)
  {
    // FIX 2: Zatrzymujemy tylko jeśli buzzer był wcześniej włączony.
    // Bez tego warunku Stop() jest wołany ciągle nawet gdy PWM nigdy nie był startowany,
    // co może powodować nieokreślony stan pinu (zablokowany HIGH = 3.3V).
    if (bz->is_on)
    {
      HAL_TIM_PWM_Stop(htim, channel);
      bz->is_on = 0;
    }
    return;
  }

  uint32_t now = HAL_GetTick();
  if (now - bz->last_tick >= bz->beep_interval)
  {
    bz->last_tick = now;
    if (bz->is_on)
    {
      HAL_TIM_PWM_Stop(htim, channel);
      bz->is_on = 0;
    }
    else
    {
      // FIX: Ustawiamy ARR (period) a potem CCR (wypełnienie = 50%)
      __HAL_TIM_SET_AUTORELOAD(htim, bz->pitch_arr);
      __HAL_TIM_SET_COMPARE(htim, channel, bz->pitch_arr / 2);
      HAL_TIM_PWM_Start(htim, channel);
      bz->is_on = 1;
    }
  }
}

void Set_Buzzer(Buzzer_t *bz, uint8_t row, uint16_t distance)
{
  bz->active = 1;

  /*
   * Mapowanie wiersza na ton (ARR dla timera 1MHz):
   *   row == 0 → góra    → ARR=499  → 1MHz/500  = 2000 Hz (wysoki)
   *   row == 1 → środek  → ARR=999  → 1MHz/1000 = 1000 Hz (średni)
   *   row >= 2 → dół     → ARR=1999 → 1MHz/2000 = 500 Hz  (niski)
   */
  if (row == 0)
    bz->pitch_arr = 499;
  else if (row == 1)
    bz->pitch_arr = 999;
  else
    bz->pitch_arr = 1999;

  /*
   * Mapowanie dystansu na tempo pikania:
   *   < 400 mm  → 50 ms  (prawie ciągły)
   *   < 800 mm  → 150 ms (szybki)
   *   >= 800 mm → 400 ms (wolny)
   */
  if (distance < 400)
    bz->beep_interval = 50;
  else if (distance < 800)
    bz->beep_interval = 150;
  else
    bz->beep_interval = 400;
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
