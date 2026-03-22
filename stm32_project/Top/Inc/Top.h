#pragma once

#include "53l8a1_ranging_sensor.h"
#include "NanoEdgeAI.h"

#define TOF_SENSOR_ODR 15u
#define RESULT_VALUES_NB 1u
#define TOF_RESOLUTION VL53L8CX_RESOLUTION_8X8
#define SIGNAL_SIZE (TOF_RESOLUTION * RESULT_VALUES_NB)
#define DISTANCE_MAX 2000u
#define SKIPPED_RESULTS_NB 2u

void Top_Init();
void Top_Loop();

void HAL_GPIO_EXTI_Callback(uint16_t);
