#include "Imu_Reader.h"
#include "stm32l4xx_hal_uart.h"
#include <string.h>

struct SensorData receivedData;

uint8_t rx_byte;            // Holds the single byte we just received
uint8_t sync_state = 0;     // Tracks our progress finding the header
uint8_t payload_buffer[32]; // Holds the 32 bytes of actual data
uint8_t payload_index = 0;  // Tracks how many payload bytes we have collected

UART_HandleTypeDef* uart;

void Imu_Reader_Init(UART_HandleTypeDef* u)
{
    uart = u;
    HAL_UART_Receive_IT(uart, &rx_byte, 1);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    HAL_UART_Receive_IT(uart, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == uart)
    {
        if (sync_state == 0 && rx_byte == 0xDD)
        {
            sync_state = 1;
        }
        else if (sync_state == 1 && rx_byte == 0xCC)
        {
            sync_state = 2;
        }
        else if (sync_state == 2 && rx_byte == 0xBB)
        {
            sync_state = 3;
        }
        else if (sync_state == 3 && rx_byte == 0xAA)
        {
            sync_state = 4;    // Move to payload collection mode
            payload_index = 0; // Reset the payload counter
        }
        else if (sync_state == 4)
        {
            payload_buffer[payload_index] = rx_byte;
            payload_index++;

            if (payload_index >= 26)
            {
            	memcpy((void*)&receivedData.gyroX, payload_buffer, 26);
                sync_state = 0;
            }
        }
        else
        {
            sync_state = 0;
        }
        HAL_UART_Receive_IT(uart, &rx_byte, 1);
    }
}
