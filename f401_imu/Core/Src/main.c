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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h> // Needed for printf
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Includes */
#include "lsm6dsv16x.h"
#include "sths34pf80.h"

extern I2C_HandleTypeDef hi2c1;

STHS34PF80_Object_t sths_obj;
LSM6DSV16X_Object_t lsm_obj;

#define STHS34PF80_I2C_ADD 0xB4
#define LSM6DSV16X_I2C_ADD 0xD6

/* Platform specific wrapper functions */
int32_t Platform_WriteReg(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
    if (HAL_I2C_Mem_Write(&hi2c1, Addr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 1000) == HAL_OK)
    {
        return 0; // Success
    }
    return -1; // Error
}

int32_t Platform_ReadReg(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
    if (HAL_I2C_Mem_Read(&hi2c1, Addr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 1000) == HAL_OK)
    {
        return 0; // Success
    }
    return -1; // Error
}

int32_t Platform_Init(void) { return 0; }
int32_t Platform_DeInit(void) { return 0; }
void BSP_Sensors_Init(void)
{
    STHS34PF80_IO_t sths_io = {0};
    sths_io.BusType = STHS34PF80_I2C_BUS;
    sths_io.Address = STHS34PF80_I2C_ADD;
    sths_io.Init = Platform_Init;
    sths_io.DeInit = Platform_DeInit;
    sths_io.WriteReg = Platform_WriteReg;
    sths_io.ReadReg = Platform_ReadReg;
    sths_io.GetTick = (STHS34PF80_GetTick_Func)HAL_GetTick;
    sths_io.Delay = HAL_Delay;

    // Link IO and Initialize Object
    STHS34PF80_RegisterBusIO(&sths_obj, &sths_io);
    if (STHS34PF80_Init(&sths_obj) == STHS34PF80_OK)
    {
        // Enable desired features
        STHS34PF80_TEMP_Enable(&sths_obj);
        STHS34PF80_TEMP_SetOutputDataRate(&sths_obj, 15.0f);

        STHS34PF80_SetAvgTmos(&sths_obj, 32);
    }

    /* -----------------------------------------------------------------------
     */
    /* 2. Initialize LSM6DSV16X (IMU: Accelerometer + Gyroscope) */
    /* -----------------------------------------------------------------------
     */
    LSM6DSV16X_IO_t lsm_io = {0};
    lsm_io.BusType = LSM6DSV16X_I2C_BUS; // Defined in your header as 0U
    lsm_io.Address = LSM6DSV16X_I2C_ADD;
    lsm_io.Init = Platform_Init;
    lsm_io.DeInit = Platform_DeInit;
    lsm_io.WriteReg = Platform_WriteReg;
    lsm_io.ReadReg = Platform_ReadReg;
    lsm_io.GetTick = (LSM6DSV16X_GetTick_Func)HAL_GetTick;
    lsm_io.Delay = HAL_Delay;

    // Link IO and Initialize Object
    LSM6DSV16X_RegisterBusIO(&lsm_obj, &lsm_io);
    if (LSM6DSV16X_Init(&lsm_obj) == LSM6DSV16X_OK)
    {
        // Enable desired features
        LSM6DSV16X_ACC_Enable(&lsm_obj);
        LSM6DSV16X_GYRO_Enable(&lsm_obj);
        // Wake up the IMU by setting the Output Data Rate (e.g., 104 Hz)
        LSM6DSV16X_ACC_SetOutputDataRate(&lsm_obj, 104.0f);
        LSM6DSV16X_GYRO_SetOutputDataRate(&lsm_obj, 104.0f);
    }
}
void Scan_I2C_Bus(void)
{
    printf("Scanning I2C bus...\r\n");
    uint8_t devices_found = 0;

    /* Loop through all possible 7-bit addresses (1 to 127) */
    for (uint8_t i = 1; i < 128; i++)
    {
        /* STM32 HAL requires the address to be shifted left by 1 bit */
        uint16_t hal_address = (uint16_t)(i << 1);

        /* Ping the address (3 trials, 10ms timeout) */
        if (HAL_I2C_IsDeviceReady(&hi2c1, hal_address, 3, 10) == HAL_OK)
        {
            printf("-> Device found! 7-bit Addr: 0x%02X | STM32 8-bit Addr: "
                   "0x%02X\r\n",
                   i, hal_address);
            devices_found++;
        }
    }

    if (devices_found == 0)
    {
        printf("No I2C devices found. Check cables, power, and pull-up "
               "resistors!\r\n");
    }
    else
    {
        printf("Scan complete. %d device(s) found.\r\n", devices_found);
    }
    printf("----------------------------------\r\n");
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

    /* MCU
     * Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the
     * Systick. */
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
    MX_I2C1_Init();
    /* USER CODE BEGIN 2 */

    //  Scan_I2C_Bus();

    BSP_Sensors_Init();

    printf("TOP\r\n");
    /* Variables to hold STHS34PF80 data */
    float ambient_temp = 0.0f;
    float object_temp = 0.0f;
    uint8_t sths_ready = 0;

    /* Variables to hold LSM6DSV16X data */
    LSM6DSV16X_Axes_t acc_axes;
    LSM6DSV16X_Axes_t gyro_axes;
    uint8_t lsm_acc_ready = 0;
    uint8_t lsm_gyro_ready = 0;
    int16_t presence_data = 0;
    uint8_t presence_flag = 0;

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        LSM6DSV16X_ACC_Get_DRDY_Status(&lsm_obj, &lsm_acc_ready);
        if (lsm_acc_ready == 1)
        {
            LSM6DSV16X_ACC_GetAxes(&lsm_obj, &acc_axes);

            printf("ACC (g): X=%.2ld, Y=%.2ld, Z=%.2ld\r\n", acc_axes.x, acc_axes.y, acc_axes.z);
        }

        LSM6DSV16X_GYRO_Get_DRDY_Status(&lsm_obj, &lsm_gyro_ready);
        if (lsm_gyro_ready == 1)
        {
            LSM6DSV16X_GYRO_GetAxes(&lsm_obj, &gyro_axes);

            printf("GYRO (deg/s): X=%.1ld, Y=%.1ld, Z=%.1ld\r\n", gyro_axes.x, gyro_axes.y, gyro_axes.z);
        }

        STHS34PF80_TEMP_Get_DRDY_Status(&sths_obj, &sths_ready);
        if (sths_ready == 1)
        {
            STHS34PF80_TEMP_GetTemperature(&sths_obj, &ambient_temp);
            STHS34PF80_GetObjectTemperature(&sths_obj, &object_temp);

            STHS34PF80_GetPresenceData(&sths_obj, &presence_data);
            STHS34PF80_GetPresenceFlag(&sths_obj, &presence_flag);

            // Calculate differential heat signature
            float diff_temp = (float)presence_data / 2000.0f;

            printf("STHS: Room=%.1f C | Heat Delta=%.2f C | Flag=%d\r\n", ambient_temp, diff_temp, presence_flag);
        }

        HAL_Delay(500);
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
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

    /* USER CODE BEGIN I2C1_Init 0 */

    /* USER CODE END I2C1_Init 0 */

    /* USER CODE BEGIN I2C1_Init 1 */

    /* USER CODE END I2C1_Init 1 */
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN I2C1_Init 2 */

    /* USER CODE END I2C1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

    /* USER CODE BEGIN USART2_Init 0 */

    /* USER CODE END USART2_Init 0 */

    /* USER CODE BEGIN USART2_Init 1 */

    /* USER CODE END USART2_Init 1 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART2_Init 2 */

    /* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* USER CODE BEGIN MX_GPIO_Init_1 */

    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : B1_Pin */
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pin : LD2_Pin */
    GPIO_InitStruct.Pin = LD2_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
    while (1)
    {
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
