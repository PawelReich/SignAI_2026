#include "Top.h"
#include "Tof.h"
#include "MotionController.h"
#include "Imu_Reader.h"
#include "Buzzer.h"
#include <stdio.h>

#include <stdint.h>


MotionController mc;

RANGING_SENSOR_Capabilities_t Cap;
RANGING_SENSOR_ProfileConfig_t Profile;

uint32_t resultCnt = 0;

uint8_t tofReady = 0;

float input_user_buffer[SIGNAL_SIZE] = {0};

float probabilities[NEAI_NUMBER_OF_CLASSES] = {0};
int id_class = 0;
enum neai_state neai_state_var;

Buzzer_t buzzer_left  = {GPIOA, GPIO_PIN_6};
Buzzer_t buzzer_right = {GPIOC, GPIO_PIN_3};

float acc_x, acc_y, acc_z, gyro_x;

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

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_4)
        tofReady = 1;
}

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
            Buzzer_Tone(&buzzer_left, 250, 150);
        if (buzzer == 1 || buzzer == 2)
            Buzzer_Tone(&buzzer_right, 2500, 150);
    }
    else
    {
        /* Jedno piknięcie — kolejne wywołanie przyjdzie z następną klatką */
        if (buzzer == 0 || buzzer == 2)
            Buzzer_Tone(&buzzer_left, 250, beep_ms);
        if (buzzer == 1 || buzzer == 2)
            Buzzer_Tone(&buzzer_right, 2500, beep_ms);
        HAL_Delay(pause_ms);
    }
}

int32_t FillBuffer(float *buffer)
{
    RANGING_SENSOR_Result_t Result;
    int32_t status = 0;

    if (tofReady)
    {
        tofReady = 0;
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

    pos += snprintf(&out_buf[pos], sizeof(out_buf) - pos, "\nGyro X:%ld Y:%ld Z:%ld\nAccl X:%ld Y:%ld Z:%ld\n", receivedData.gyroX, receivedData.gyroY, receivedData.gyroZ, receivedData.accelX,
                    receivedData.accelY, receivedData.accelZ);

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
            PrintBuffer(input_user_buffer);

            neai_classification(input_user_buffer, probabilities, &id_class);
            printf("# CLASS:%d CONF:%.0f%%\r\n", id_class, probabilities[id_class] * 100.0f);
            printf("# -> %s\r\n", neai_get_class_name(id_class));

            MotionController_Update(
                &mc,
                (float)receivedData.accelX,
                (float)receivedData.accelY,
                (float)receivedData.accelZ,
                (float)receivedData.gyroY
            );

            // Wyznacz co wolno buzzeć na podstawie kąta
            float pitch = mc.pitch_angle;
            bool mute_all = mc.is_sitting ||  mc.is_frozen;
            bool mute_bot = (fabsf(pitch - 90.0f) >= PITCH_MUTE_BOTTOM);

            if (mute_all)
            {
                // nic nie rób
            }
            else
            {
                uint32_t d = 0;
                switch (id_class)
                {
                    case 0: /* class_free — przeszkoda wszędzie, oba buzzery */
                        d = GetMinDistance(input_user_buffer, 0, 7);
                        Buzzer_Proximity(d, 1000, 2);
                        break;

                    case 1: /* class_left — przeszkoda po lewej */
                        if (!mute_bot)
                            d = GetMinDistance(input_user_buffer, 4, 7); // cała lewa strona
                        else
                            d = GetMinDistance(input_user_buffer, 4, 7); // tylko górne rzędy wystarczą
                        Buzzer_Proximity(d, 1000, 0); // lewy buzzer
                        break;

                    case 2: /* class_right — przeszkoda po prawej */
                        d = GetMinDistance(input_user_buffer, 0, 3);
                        Buzzer_Proximity(d, 1000, 1); // prawy buzzer
                        break;

                    case 3: /* class_wall — ściana przed nami, oba */
                        d = GetMinDistance(input_user_buffer, 0, 7);
                        Buzzer_Proximity(d, 1000, 2);
                        break;

                    default:
                        break;
                }
            }
        }
    }
    else
    {
        printf("Waiting for valid data...\r\n");
    }
}

void Top_Init()
{
    Imu_Reader_Init();

    ToF_Init();
    ToF_ProfileConfig(SIGNAL_SIZE);
    ToF_Start();

    neai_state_var = neai_classification_init();
    printf("NEAI init: %d\r\n", neai_state_var);

    MotionController_Init(&mc);

    /* Krótki sygnał startowy */
    Buzzer_Beep(&buzzer_left, 2000, 80, 60, 2);
    Buzzer_Beep(&buzzer_right, 2000, 80, 60, 2);

}

void Top_Loop()
{
    Log();
}
