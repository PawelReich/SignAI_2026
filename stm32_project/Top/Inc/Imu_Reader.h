#pragma once

#include "main.h"
#include <stdint.h>

void Imu_Reader_Init();

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

extern struct SensorData receivedData;

struct SensorData
{
    int32_t header;
    int32_t gyroX;
    int32_t gyroY;
    int32_t gyroZ;

    int32_t accelX;
    int32_t accelY;
    int32_t accelZ;

    int16_t presenceValue;
};
