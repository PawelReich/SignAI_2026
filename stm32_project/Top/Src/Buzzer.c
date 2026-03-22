#include "Buzzer.h"

void Buzzer_Tone(Buzzer_t *buzzer, uint32_t freq_hz, uint32_t duration_ms)
{
    /* Safety check for null pointer or 0 frequency */
    if (buzzer == NULL || freq_hz == 0)
    {
        HAL_Delay(duration_ms);
        return;
    }

    uint32_t half_period_us = 500000u / freq_hz;
    uint32_t cycles = (freq_hz * duration_ms) / 1000u;

    for (uint32_t i = 0; i < cycles; i++)
    {
        HAL_GPIO_WritePin(buzzer->port, buzzer->pin, GPIO_PIN_SET);
        volatile uint32_t d = half_period_us * 10u;
        while (d--)
        {
            __NOP();
        }

        HAL_GPIO_WritePin(buzzer->port, buzzer->pin, GPIO_PIN_RESET);
        d = half_period_us * 10u;
        while (d--)
        {
            __NOP();
        }
    }
}

void Buzzer_Beep(Buzzer_t *buzzer, uint32_t freq_hz, uint32_t on_ms, uint32_t off_ms, uint8_t times)
{
    if (buzzer == NULL) return;

    for (uint8_t i = 0; i < times; i++)
    {
        Buzzer_Tone(buzzer, freq_hz, on_ms);
        if (i < times - 1)
            HAL_Delay(off_ms);
    }
}
