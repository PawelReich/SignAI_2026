#include "imu.h"

static uint32_t lastTick = 0;

STHS34PF80_Object_t sths_obj;
LSM6DSV16X_Object_t lsm_obj;

uint8_t lsm_acc_ready = 0;
uint8_t lsm_gyro_ready = 0;
uint8_t sths_ready = 0;

int16_t presence_data = 0;
LSM6DSV16X_Axes_t acc_axes;
LSM6DSV16X_Axes_t gyro_axes;

I2C_HandleTypeDef *IMU_I2C;
UART_HandleTypeDef *MASTER_UART;

void Init_Sensors(void);

void Imu_Init(I2C_HandleTypeDef* i2c, UART_HandleTypeDef* uart)
{
    lastTick = HAL_GetTick();

    IMU_I2C = i2c;
    MASTER_UART = uart;

    Init_Sensors();
}

void Imu_Loop()
{
    LSM6DSV16X_ACC_Get_DRDY_Status(&lsm_obj, &lsm_acc_ready);
    if (lsm_acc_ready == 1)
    {
        LSM6DSV16X_ACC_GetAxes(&lsm_obj, &acc_axes);
    }

    LSM6DSV16X_GYRO_Get_DRDY_Status(&lsm_obj, &lsm_gyro_ready);
    if (lsm_gyro_ready == 1)
    {
        LSM6DSV16X_GYRO_GetAxes(&lsm_obj, &gyro_axes);
    }

    STHS34PF80_TEMP_Get_DRDY_Status(&sths_obj, &sths_ready);
    if (sths_ready == 1)
    {
        STHS34PF80_GetPresenceData(&sths_obj, &presence_data);
    }

    uint32_t newTick = HAL_GetTick();
    if (newTick - lastTick > 100)
    {
        lastTick = newTick;

        struct SensorData payload;
        payload.header = 0xAABBCCDD;
        payload.accelX = acc_axes.x;
        payload.accelY = acc_axes.y;
        payload.accelZ = acc_axes.z;
        payload.gyroX = gyro_axes.x;
        payload.gyroY = gyro_axes.y;
        payload.gyroZ = gyro_axes.z;
        payload.presenceValue = presence_data;

        HAL_UART_Transmit(MASTER_UART, (uint8_t*)& payload, sizeof(payload), 100);
    }

}


int32_t Platform_WriteReg(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
    if (HAL_I2C_Mem_Write(IMU_I2C, Addr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 1000) == HAL_OK)
    {
        return 0; // Success
    }
    return -1; // Error
}

int32_t Platform_ReadReg(uint16_t Addr, uint16_t Reg, uint8_t *pData, uint16_t Length)
{
    if (HAL_I2C_Mem_Read(IMU_I2C, Addr, Reg, I2C_MEMADD_SIZE_8BIT, pData, Length, 1000) == HAL_OK)
    {
        return 0; // Success
    }
    return -1; // Error
}

int32_t Platform_Init(void) { return 0; }
int32_t Platform_DeInit(void) { return 0; }

void Init_Sensors(void)
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

    STHS34PF80_RegisterBusIO(&sths_obj, &sths_io);
    if (STHS34PF80_Init(&sths_obj) == STHS34PF80_OK)
    {
        STHS34PF80_TEMP_Enable(&sths_obj);
        STHS34PF80_TEMP_SetOutputDataRate(&sths_obj, 15.0f);

        STHS34PF80_SetAvgTmos(&sths_obj, 32);
    }

    LSM6DSV16X_IO_t lsm_io = {0};
    lsm_io.BusType = LSM6DSV16X_I2C_BUS;
    lsm_io.Address = LSM6DSV16X_I2C_ADD;
    lsm_io.Init = Platform_Init;
    lsm_io.DeInit = Platform_DeInit;
    lsm_io.WriteReg = Platform_WriteReg;
    lsm_io.ReadReg = Platform_ReadReg;
    lsm_io.GetTick = (LSM6DSV16X_GetTick_Func)HAL_GetTick;
    lsm_io.Delay = HAL_Delay;

    LSM6DSV16X_RegisterBusIO(&lsm_obj, &lsm_io);
    if (LSM6DSV16X_Init(&lsm_obj) == LSM6DSV16X_OK)
    {
        LSM6DSV16X_ACC_Enable(&lsm_obj);
        LSM6DSV16X_GYRO_Enable(&lsm_obj);

        LSM6DSV16X_ACC_SetOutputDataRate(&lsm_obj, 104.0f);
        LSM6DSV16X_GYRO_SetOutputDataRate(&lsm_obj, 104.0f);
    }
}

