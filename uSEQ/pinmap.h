/*
 ------------------------------------------------------------------------------
| Copyright Dimitris Kyriakoudis and Chris Kiefer 2022.                                                    |
|                                                                              |
| This source describes Open Hardware and is licensed under the CERN-OHL-S v2. |
|                                                                              |
| You may redistribute and modify this source and make products using it under |
| the terms of the CERN-OHL-S v2 (https://ohwr.org/cern_ohl_s_v2.txt).         |
|                                                                              |
| This source is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY,          |
| INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A         |
| PARTICULAR PURPOSE. Please see the CERN-OHL-S v2 for applicable conditions.  |
|                                                                              |
| Source location: https://github.com/lnfiniteMonkeys/uSEQ                                      |
|                                                                              |
| As per CERN-OHL-S v2 section 4, should You produce hardware based on this    |
| source, You must where practicable maintain the Source Location visible      |
| on the external case of the Gizmo or other products you make using this      |
| source.                                                                      |
 ------------------------------------------------------------------------------
*/

#ifndef PINMAP_H
#define PINMAP_H

#define LED_BOARD 25

const int useq_output_pins[] = {21,20,19,18,17,16};
const int useq_output_led_pins[] = {3,2,28,27,26,22};

#define USEQ_PIN_I1 8
#define USEQ_PIN_I2 9

#define USEQ_PIN_LED_I1 5
#define USEQ_PIN_LED_I2 4
#define USEQ_PIN_LED_A1 3
#define USEQ_PIN_LED_A2 2

#define USEQ_PIN_LED_D1 28
#define USEQ_PIN_LED_D2 27
#define USEQ_PIN_LED_D3 26
#define USEQ_PIN_LED_D4 22

#define USEQ_PIN_SWITCH_M1 10
#define USEQ_PIN_SWITCH_M2 11

#define USEQ_PIN_SWITCH_T1 14
#define USEQ_PIN_SWITCH_T2 15
#define USEQ_PIN_SWITCH_R1 7

#define USEQ_PIN_ROTARYENC_A 13
#define USEQ_PIN_ROTARYENC_B 12

#endif
