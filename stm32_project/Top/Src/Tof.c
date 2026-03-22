#include "Tof.h"

void ToF_Init(void)
{
    int32_t status = 0;

    printf("ToF sensor init...\r\n");

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

void ToF_Start(void)
{
    int32_t status = VL53L8A1_RANGING_SENSOR_Start(VL53L8A1_DEV_CENTER, RS_MODE_BLOCKING_CONTINUOUS);
    if (status != BSP_ERROR_NONE)
    {
        printf("ERROR: TOF sensor start failed\r\n");
        Error_Handler();
    }
}
