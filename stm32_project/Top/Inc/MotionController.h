#pragma once
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define DT 0.050f            // Czas pętli w sekundach
#define ALPHA 0.1f           // Współczynnik filtru komplementarnego
#define EMA_BETA 0.5f        // Współczynnik filtru EMA dla drgań (0.1 - mocny, 0.9 - słaby)
#define VIB_THRESHOLD 100.0f // Próg odchyłki wektora grawitacji m mg
#define FREEZE_TIME_MS 500   // Czas zamrożenia po wstrząsie (w milisekundach)
#define FREEZE_CYCLES (uint32_t)(FREEZE_TIME_MS / (DT * 1000.0f))

#define PITCH_MUTE_BOTTOM 15.0f // Przy 15° ToF zaczyna widzieć podłogę dolnym rzędem
#define PITCH_MUTE_ALL 35.0f    // Przy 35° mute na wszystko

typedef struct
{
    float pitch_angle;
    float acc_mag_ema;
    uint32_t freeze_timer;
    bool is_frozen;
    bool is_sitting;
    float acc_z_ema;
} MotionController;

void MotionController_Init(MotionController *mc);

void MotionController_Update(MotionController *mc, float acc_x, float acc_y, float acc_z, float gyro_y);
