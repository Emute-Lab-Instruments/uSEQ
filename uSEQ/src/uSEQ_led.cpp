#include "uSEQ.h"
#include "utils.h"
#include "hardware_includes.h"



void setup_leds()
{
    DBG("uSEQ::setup_leds");

#ifndef MUSICTHING
    // pinMode(LED_BOARD, OUTPUT); // test LED
    // digitalWrite(LED_BOARD, 1);
#endif

#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_LED_AI1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_AI2, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I2, OUTPUT_2MA);
#endif

    for (int i = 0; i < (NUM_CONTINUOUS_OUTS+NUM_CONTINUOUS_OUTS); i++)
    {
        pinMode(useq_output_led_pins[i], OUTPUT_2MA);
        gpio_set_slew_rate(useq_output_led_pins[i], GPIO_SLEW_RATE_SLOW);
    }
}

void uSEQ::led_animation()
{
    int ledDelay = 30;
#ifdef MUSICTHING
    ledDelay = 40;
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(useq_output_led_pins[0], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[2], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[4], 1);
        digitalWrite(useq_output_led_pins[0], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[5], 1);
        digitalWrite(useq_output_led_pins[2], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[3], 1);
        digitalWrite(useq_output_led_pins[4], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 1);
        digitalWrite(useq_output_led_pins[5], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[3], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 0);
        ledDelay -= 3;
    }
#endif
#ifdef USEQHARDWARE_0_2
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(USEQ_PIN_LED_I1, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A1, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D1, 1);
        digitalWrite(USEQ_PIN_LED_I1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D3, 1);
        digitalWrite(USEQ_PIN_LED_A1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D4, 1);
        digitalWrite(USEQ_PIN_LED_D1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D2, 1);
        digitalWrite(USEQ_PIN_LED_D3, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A2, 1);
        digitalWrite(USEQ_PIN_LED_D4, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 1);
        digitalWrite(USEQ_PIN_LED_D2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 0);
        delay(ledDelay);
        ledDelay -= 3;
    }
#endif
#ifdef USEQHARDWARE_1_0
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(USEQ_PIN_LED_AI1, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_AI2, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A1, 1);
        digitalWrite(USEQ_PIN_LED_AI1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A2, 1);
        digitalWrite(USEQ_PIN_LED_AI2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A3, 1);
        digitalWrite(USEQ_PIN_LED_A1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D3, 1);
        digitalWrite(USEQ_PIN_LED_A2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D2, 1);
        digitalWrite(USEQ_PIN_LED_A3, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D1, 1);
        digitalWrite(USEQ_PIN_LED_D3, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 1);
        digitalWrite(USEQ_PIN_LED_D2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I1, 1);
        digitalWrite(USEQ_PIN_LED_D1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I1, 0);
        delay(ledDelay);
        ledDelay -= 3;
    }
#endif
#ifdef USEQHARDWARE_EXPANDER_OUT_0_1
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(useq_output_led_pins[0], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[0], 0);
        digitalWrite(useq_output_led_pins[2], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 0);
        digitalWrite(useq_output_led_pins[3], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[2], 0);
        digitalWrite(useq_output_led_pins[4], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[3], 0);
        digitalWrite(useq_output_led_pins[5], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[4], 0);
        digitalWrite(useq_output_led_pins[6], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[5], 0);
        digitalWrite(useq_output_led_pins[7], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[6], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[7], 0);

        ledDelay -= 3;
    }
#endif
}