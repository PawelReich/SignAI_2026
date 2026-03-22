#include "MotionController.h"

#include <stdio.h>

void MotionController_Init(MotionController* mc) {
    mc->pitch_angle = 0.0f;
    mc->acc_mag_ema = 1000.0f;
    mc->freeze_timer = 0;
    mc->is_frozen = false;
    mc->is_sitting = false;
    mc->acc_z_ema = 1000.0f;  // 1g w spoczynku
}

void MotionController_Update(MotionController* mc, float acc_x, float acc_y, float acc_z, float gyro_y) {

    // WYKRYWANIE DRGAŃ
    float acc_magnitude = sqrtf(acc_x * acc_x + acc_y * acc_y + acc_z * acc_z);

    mc->acc_mag_ema = (EMA_BETA * acc_magnitude) + ((1.0f - EMA_BETA) * mc->acc_mag_ema);

    float acc_diff = fabsf(mc->acc_mag_ema - 1000.0f);

    if (acc_diff > VIB_THRESHOLD) {
        mc->freeze_timer = FREEZE_CYCLES;
    }

    if (mc->freeze_timer > 0) {
        mc->freeze_timer--;
        mc->is_frozen = true;
    } else {
        mc->is_frozen = false;
    }

    // OBLICZENIE KĄTA Z AKCELEROMETRU (w stopniach)
    float acc_pitch = atan2f(acc_x, acc_z) * (180.0f / 3.14159265f);

    // Zamiana na stopnie w czasie dt
    float gyro_delta_deg = (gyro_y / 1000.0f) * DT;

    // FILTR KOMPLEMENTARNY
    mc->pitch_angle = ALPHA * (mc->pitch_angle + gyro_delta_deg) + (1.0f - ALPHA) * acc_pitch;

    // WYKRYWANIE SIADANIA/WSTAWANIA
	#define SIT_DOWN_THRESHOLD   200.0f
	#define STAND_UP_THRESHOLD  -200.0f
	#define ACC_Z_EMA_BETA       0.3f

    mc->acc_z_ema = (ACC_Z_EMA_BETA * acc_z) + ((1.0f - ACC_Z_EMA_BETA) * mc->acc_z_ema);

    float acc_z_delta_raw = acc_z - 1000.0f;

    if (!mc->is_sitting && acc_z_delta_raw > SIT_DOWN_THRESHOLD) {
        mc->is_sitting = true;
        printf(">> SITTING\r\n");
    } else if (mc->is_sitting && acc_z_delta_raw < STAND_UP_THRESHOLD) {
        mc->is_sitting = false;
        printf(">> STANDING\r\n");
    }

    printf("pitch=%.1f sitting=%d acc_z_delta=%.1f frozen=%d\r\n",
           mc->pitch_angle, mc->is_sitting, acc_z_delta_raw, mc->is_frozen);
}
