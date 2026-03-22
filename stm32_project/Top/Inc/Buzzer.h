#pragma once

#include "main.h"
#include <stdint.h>

/* Buzzer Object Structure */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} Buzzer_t;

/* Externally accessible buzzer instances */
extern Buzzer_t buzzer_left;
extern Buzzer_t buzzer_right;

#define BUZZER_FREQ_HZ 2000u

void Buzzer_Tone(Buzzer_t *buzzer, uint32_t freq_hz, uint32_t duration_ms);
void Buzzer_Beep(Buzzer_t *buzzer, uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms, uint8_t times);
