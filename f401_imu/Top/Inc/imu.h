#pragma once

#include "lsm6dsv16x.h"
#include "sths34pf80.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"
#include "stm32f4xx_hal_uart.h"

#define STHS34PF80_I2C_ADD 0xB4
#define LSM6DSV16X_I2C_ADD 0xD6

void Imu_Init(I2C_HandleTypeDef *i2c, UART_HandleTypeDef *uart);
void Imu_Loop();

struct SensorData
{
    uint32_t header; // <-- Added header
    int32_t gyroX;
    int32_t gyroY;
    int32_t gyroZ;
    int32_t accelX;
    int32_t accelY;
    int32_t accelZ;
    int32_t presenceAmbient;
    int32_t presenceDiff;
};
