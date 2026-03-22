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
#include "gpio.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "53l8a1_ranging_sensor.h"
#include "NanoEdgeAI.h"
#include "stdio.h"
#include <stdbool.h>
#include <stdint.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
    LOG,
} States_t;

typedef struct
{
    float pitch_angle;
    float acc_mag_ema;
    uint32_t freeze_timer;
    bool is_frozen;
    bool is_sitting; // ← NOWE
    float acc_z_ema; // ← NOWE - wygładzona oś Z do wykrywania trendu
} MotionController;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define TOF_SENSOR_ODR 15u
#define RESULT_VALUES_NB 1u
#define TOF_RESOLUTION VL53L8CX_RESOLUTION_8X8
#define SIGNAL_SIZE (TOF_RESOLUTION * RESULT_VALUES_NB)
#define DISTANCE_MAX 2000u
#define SKIPPED_RESULTS_NB 2u
#define LED_LOG_OK_PERIOD 75u
#define LED_LOG_NOK_PERIOD 25u

/* Buzzer LEWY — PA6 (CN10 pin 13) */
#define BUZZER_GPIO_PORT GPIOA
#define BUZZER_PIN GPIO_PIN_6

/* Buzzer PRAWY — PB0 (CN10 pin 31) */
#define BUZZER_R_PORT GPIOC
#define BUZZER_R_PIN GPIO_PIN_3

#define BUZZER_FREQ_HZ 2000u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define DT 0.050f            // Czas pętli w sekundach
#define ALPHA 0.1f           // Współczynnik filtru komplementarnego
#define EMA_BETA 0.5f        // Współczynnik filtru EMA dla drgań (0.1 - mocny, 0.9 - słaby)
#define VIB_THRESHOLD 100.0f // Próg odchyłki wektora grawitacji m mg
#define FREEZE_TIME_MS 500   // Czas zamrożenia po wstrząsie (w milisekundach)
#define FREEZE_CYCLES (uint32_t)(FREEZE_TIME_MS / (DT * 1000.0f))

// Progi pochylenia
#define PITCH_MUTE_BOTTOM 15.0f // Przy 15° ToF zaczyna widzieć podłogę dolnym rzędem
#define PITCH_MUTE_ALL 35.0f    // Przy 35° mute na wszystko
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

MotionController mc;
float acc_x, acc_y, acc_z, gyro_x;
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
int id_class = 0;
enum neai_state neai_state_var;
enum neai_state neai_2_state_var;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}
/* USER CODE BEGIN PFP */

void ToF_Init(void);
void ToF_ProfileConfig(uint8_t resolution);
void ToF_Start(void);
int32_t FillBuffer(float *buffer);
void PrintBuffer(float *buffer);
void Log(void);

void Buzzer_Init(void);
void Buzzer_Tone(uint32_t freq_hz, uint32_t duration_ms);
void Buzzer_Beep(uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms, uint8_t times);
void Buzzer_R_Tone(uint32_t freq_hz, uint32_t duration_ms);
void Buzzer_R_Beep(uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms, uint8_t times);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
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
uint8_t rx_byte;            // Holds the single byte we just received
uint8_t sync_state = 0;     // Tracks our progress finding the header
uint8_t payload_buffer[26]; // Holds the 32 bytes of actual data
uint8_t payload_index = 0;  // Tracks how many payload bytes we have collected

volatile struct SensorData receivedData;

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    // If the UART crashes or overflows, completely reset the listening process
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) // Replace with your UART instance
    {
        // --- STEP 1: HUNTING FOR THE HEADER ---
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
            // We found the full header!
            sync_state = 4;    // Move to payload collection mode
            payload_index = 0; // Reset the payload counter
        }

        // --- STEP 2: HEADER FOUND, COLLECTING PAYLOAD ---
        else if (sync_state == 4)
        {
            payload_buffer[payload_index] = rx_byte;
            payload_index++;

            // Did we get all 32 bytes of the payload?
            if (payload_index >= 26)
            {
                // We got a complete, perfectly synced packet!
                // Copy the 32 bytes directly into our struct, starting at gyroX
                // (We skip over the 'header' variable in the struct using &myReceivedData.gyroX)
                memcpy((void *)&receivedData.gyroX, payload_buffer, 26);

                // You can now use your data! e.g., myReceivedData.accelZ

                // Reset the state machine to hunt for the next packet header
                sync_state = 0;
            }
        }

        // --- STEP 3: GARBAGE DATA OR MISMATCH ---
        else
        {
            // If the sequence breaks, reset and start looking for 0xDD again
            sync_state = 0;
        }
        // CRITICAL: Always ask the hardware to listen for the NEXT single byte
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}

// Inicjalizacja struktury
void MotionController_Init(MotionController *mc)
{
    mc->pitch_angle = 0.0f;
    mc->acc_mag_ema = 1000.0f;
    mc->freeze_timer = 0;
    mc->is_frozen = false;
    mc->is_sitting = false;
    mc->acc_z_ema = 1000.0f; // 1g w spoczynku
}

// funkcja wykonywana co DT w main
void MotionController_Update(MotionController *mc, float acc_x, float acc_y, float acc_z, float gyro_y)
{

    // WYKRYWANIE DRGAŃ
    float acc_magnitude = sqrtf(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);

    mc->acc_mag_ema = (EMA_BETA * acc_magnitude) + ((1.0f - EMA_BETA) * mc->acc_mag_ema);

    float acc_diff = fabsf(mc->acc_mag_ema - 1000.0f);

    if (acc_diff > VIB_THRESHOLD)
    {
        mc->freeze_timer = FREEZE_CYCLES;
    }

    if (mc->freeze_timer > 0)
    {
        mc->freeze_timer--;
        mc->is_frozen = true;
    }
    else
    {
        mc->is_frozen = false;
    }

    // OBLICZENIE KĄTA Z AKCELEROMETRU (w stopniach)
    float acc_pitch = atan2f(acc_x, acc_z) * (180.0f / 3.14159265f);

    // Zamiana na stopnie w czasie dt
    float gyro_delta_deg = (gyro_y / 1000.0f) * DT;

    // FILTR KOMPLEMENTARNY
    mc->pitch_angle = ALPHA * (mc->pitch_angle + gyro_delta_deg) + (1.0f - ALPHA) * acc_pitch;

// WYKRYWANIE SIADANIA/WSTAWANIA
#define SIT_DOWN_THRESHOLD 200.0f
#define STAND_UP_THRESHOLD -200.0f
#define ACC_Z_EMA_BETA 0.3f

    mc->acc_z_ema = (ACC_Z_EMA_BETA * acc_z) + ((1.0f - ACC_Z_EMA_BETA) * mc->acc_z_ema);

    float acc_z_delta_raw = acc_z - 1000.0f;

    if (!mc->is_sitting && acc_z_delta_raw > SIT_DOWN_THRESHOLD)
    {
        mc->is_sitting = true;
        printf(">> SITTING\r\n");
    }
    else if (mc->is_sitting && acc_z_delta_raw < STAND_UP_THRESHOLD)
    {
        mc->is_sitting = false;
        printf(">> STANDING\r\n");
    }

    //    printf("pitch=%.1f sitting=%d acc_z_delta=%.1f frozen=%d\r\n",
    //           mc->pitch_angle, mc->is_sitting, acc_z_delta_raw, mc->is_frozen);
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
    MX_USART1_UART_Init();
    /* USER CODE BEGIN 2 */

    Buzzer_Init();

    ToF_Init();
    ToF_ProfileConfig(SIGNAL_SIZE);
    ToF_Start();

    neai_state_var = neai_classification_init();
    printf("NEAI init: %d\r\n", neai_state_var);

    MotionController_Init(&mc);

    /* Krótki sygnał startowy */
    Buzzer_Beep(2000, 80, 60, 2);
    Buzzer_R_Beep(2000, 80, 60, 2);

    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

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
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
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

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
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

uint32_t GetMinDistance(float *buffer, uint8_t col_start, uint8_t col_end)
{
    uint32_t min_dist = DISTANCE_MAX;
    for (uint8_t row = 0; row < 8; row++)
    {
        for (uint8_t col = col_start; col <= col_end; col++)
        {
            uint32_t v = (uint32_t)buffer[row * 8 + col];
            if (v < min_dist)
                min_dist = v;
        }
    }
    return min_dist;
}
void Buzzer_Proximity(uint32_t dist_mm, uint32_t dist_max_mm, uint8_t buzzer)
{
    if (dist_mm >= dist_max_mm)
        return; /* za daleko — cisza */
    uint32_t pause_ms;
    if (dist_mm < 100)
        pause_ms = 0; /* ciągły ton — bardzo blisko */
    else
        pause_ms = (dist_mm * 500) / dist_max_mm;
    uint32_t beep_ms = 40; /* długość pojedynczego piknięcia */
    if (pause_ms == 0)
    {
        /* Ciągły ton — przeszkoda krytycznie blisko */
        if (buzzer == 0 || buzzer == 2)
            Buzzer_Tone(250, 150);
        if (buzzer == 1 || buzzer == 2)
            Buzzer_R_Tone(2500, 150);
    }
    else
    {
        /* Jedno piknięcie — kolejne wywołanie przyjdzie z następną klatką */
        if (buzzer == 0 || buzzer == 2)
            Buzzer_Tone(250, beep_ms);
        if (buzzer == 1 || buzzer == 2)
            Buzzer_R_Tone(2500, beep_ms);
        HAL_Delay(pause_ms);
    }
}
void Buzzer_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    /* Lewy — PA6 */
    GPIO_InitStruct.Pin = BUZZER_PIN;
    HAL_GPIO_Init(BUZZER_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_PIN, GPIO_PIN_RESET);

    /* Prawy — PB0 */
    GPIO_InitStruct.Pin = BUZZER_R_PIN;
    HAL_GPIO_Init(BUZZER_R_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BUZZER_R_PORT, BUZZER_R_PIN, GPIO_PIN_RESET);
}

/* ── Buzzer LEWY (PA6) ────────────────────────────────────────────────── */

void Buzzer_Tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0)
    {
        HAL_Delay(duration_ms);
        return;
    }
    uint32_t half_period_us = 500000u / freq_hz;
    uint32_t cycles = (freq_hz * duration_ms) / 1000u;
    for (uint32_t i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_PIN, GPIO_PIN_SET);
        volatile uint32_t d = half_period_us * 10u;
        while (d--)
        {
            __NOP();
        }
        HAL_GPIO_WritePin(BUZZER_GPIO_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        d = half_period_us * 10u;
        while (d--)
        {
            __NOP();
        }
    }
}

void Buzzer_Beep(uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms, uint8_t times)
{
    for (uint8_t i = 0; i < times; i++)
    {
        Buzzer_Tone(freq_hz, on_ms);
        if (i < times - 1)
            HAL_Delay(off_ms);
    }
}

/* ── Buzzer PRAWY (PB0) ───────────────────────────────────────────────── */

void Buzzer_R_Tone(uint32_t freq_hz, uint32_t duration_ms)
{
    if (freq_hz == 0)
    {
        HAL_Delay(duration_ms);
        return;
    }
    uint32_t half_period_us = 500000u / freq_hz;
    uint32_t cycles = (freq_hz * duration_ms) / 1000u;
    for (uint32_t i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(BUZZER_R_PORT, BUZZER_R_PIN, GPIO_PIN_SET);
        volatile uint32_t d = half_period_us * 10u;
        while (d--)
        {
            __NOP();
        }
        HAL_GPIO_WritePin(BUZZER_R_PORT, BUZZER_R_PIN, GPIO_PIN_RESET);
        d = half_period_us * 10u;
        while (d--)
        {
            __NOP();
        }
    }
}

void Buzzer_R_Beep(uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms, uint8_t times)
{
    for (uint8_t i = 0; i < times; i++)
    {
        Buzzer_R_Tone(freq_hz, on_ms);
        if (i < times - 1)
            HAL_Delay(off_ms);
    }
}

/* ────────────────────────────────────────────────────────────────────── */

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

    Profile.TimingBudget = 30;
    Profile.Frequency = TOF_SENSOR_ODR;
    Profile.EnableSignal = 1;
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

static const uint8_t heatmap_lut[24] = {16, 17, 18, 19, 20, 21, 27, 33, 39, 45, 51, 50, 49, 48, 47, 46, 118, 154, 190, 226, 214, 202, 196, 124};

/* Fast mapping function: converts a float to a color index */
static uint8_t get_fast_color(float value)
{
    /* Set your expected Min and Max signal values here */
    const float MIN_VAL = 0.0f;
    const float MAX_VAL = 2000.0f;

    /* Normalize the float to an integer index between 0 and 23 */
    int idx = (int)(((value - MIN_VAL) / (MAX_VAL - MIN_VAL)) * 23.0f);

    /* Clamp the index to prevent array out-of-bounds crashes */
    if (idx < 0)
        idx = 0;
    if (idx > 23)
        idx = 23;

    return heatmap_lut[idx];
}

void PrintBuffer(float *buffer)
{
    char out_buf[2048];
    int pos = 0;

    /* Reset cursor to top-left smoothly */
    //    pos += snprintf(&out_buf[pos], sizeof(out_buf) - pos, "\033[1;1H\n");

    uint16_t row_width = 8;

    //    for (uint16_t i = 0; i < SIGNAL_SIZE; i++)
    //    {
    //        uint8_t color_code = get_fast_color(buffer[i]);
    //
    //        /* Add the background color and two spaces to the buffer.
    //           Notice we DO NOT reset the color here to save UART bandwidth! */
    //        pos += snprintf(&out_buf[pos], sizeof(out_buf) - pos, "\033[48;5;%dm  ", color_code);
    //
    //        /* At the end of a row, reset formatting and drop to a new line */
    //        if ((i + 1) % row_width == 0)
    //        {
    //            pos += snprintf(&out_buf[pos], sizeof(out_buf) - pos, "\033[0m\n");
    //        }
    //    }

    pos += snprintf(&out_buf[pos], sizeof(out_buf) - pos, "\nGyro X:%ld Y:%ld Z:%ld\nAccl X:%ld Y:%ld Z:%ld\nprox: %h\n", receivedData.gyroX, receivedData.gyroY, receivedData.gyroZ,
                    receivedData.accelX, receivedData.accelY, receivedData.accelZ, receivedData.presenceValue);

    /* Blast the entire buffered frame out over UART in one single shot */
    printf("%s", out_buf);
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
            printf("# CLASS:%d CONF:%.0f%%\r\n", id_class, probabilities[id_class] * 100.0f);
            printf("# -> %s\r\n", neai_get_class_name(id_class));

            MotionController_Update(&mc, (float)receivedData.accelX, (float)receivedData.accelY, (float)receivedData.accelZ, (float)receivedData.gyroY);

            // Wyznacz co wolno buzzeć na podstawie kąta
            float pitch = mc.pitch_angle;
            bool mute_all = mc.is_sitting || mc.is_frozen;
            bool mute_bot = (fabsf(pitch - 90.0f) >= PITCH_MUTE_BOTTOM);

            //            mute_all = false;
            if (mute_all)
            {
                // nic nie rób
            }
            else
            {
                uint32_t d = 0;
                switch (id_class)
                {
                    case 0: /* class_wall — przeszkoda wszędzie, oba buzzery */
                        if (receivedData.presenceValue > 5000)
                        {
                            Buzzer_Tone(523, 150);  // C5
                            Buzzer_Tone(659, 150);  // E5
                            Buzzer_Tone(784, 150);  // G5
                            Buzzer_Tone(1047, 300); // C6 - długa nuta

                            HAL_Delay(50);

                            // Prawy buzzer - echo
                            Buzzer_R_Tone(523, 150);
                            Buzzer_R_Tone(659, 150);
                            Buzzer_R_Tone(784, 150);
                            Buzzer_R_Tone(1047, 300);
                        }
                        else
                        {
                            d = GetMinDistance(input_user_buffer, 0, 7);
                            Buzzer_Proximity(d, 1000, 2);
                        }
                        break;

                    case 1:                                          /* class_left — przeszkoda po lewej */
                        d = GetMinDistance(input_user_buffer, 0, 7); // cała lewa strona
                        Buzzer_Proximity(d, 1000, 1);                // lewy buzzer
                        break;

                    case 2: /* class_right — przeszkoda po prawej */
                        d = GetMinDistance(input_user_buffer, 0, 7);
                        Buzzer_Proximity(d, 1000, 0); // prawy buzzer
                        break;

                    case 3: /* class_free — ściana przed nami, oba */
                            //                        d = GetMinDistance(input_user_buffer, 0, 7);
                            //                        Buzzer_Proximity(d, 1000, 2);
                        break;

                    default:
                        break;
                }
            }
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
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
