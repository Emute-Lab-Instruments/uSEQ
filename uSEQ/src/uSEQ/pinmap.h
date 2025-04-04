/*
 ------------------------------------------------------------------------------
| Copyright Dimitris Kyriakoudis and Chris Kiefer 2022. | | | | This source describes
Open Hardware and is licensed under the CERN-OHL-S v2. | | | | You may redistribute
and modify this source and make products using it under | | the terms of the
CERN-OHL-S v2 (https://ohwr.org/cern_ohl_s_v2.txt).         | | | | This source is
distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY,          | | INCLUDING OF
MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A         | | PARTICULAR
PURPOSE. Please see the CERN-OHL-S v2 for applicable conditions.  | | | | Source
location: https://github.com/lnfiniteMonkeys/uSEQ | | | | As per CERN-OHL-S v2
section 4, should You produce hardware based on this    | | source, You must where
practicable maintain the Source Location visible      | | on the external case of the
Gizmo or other products you make using this      | | source. |
 ------------------------------------------------------------------------------
*/

#ifndef PINMAP_H
#define PINMAP_H

/*


                                                         ___       ___
                                       .-.              (   )     (   )       .-.
 ___ .-. .-.    ___  ___      .--.    ( __)   .--.       | |_      | | .-.   ( __)
___ .-.     .--.
(   )   '   \  (   )(   )   /  _  \   (''")  /    \     (   __)    | |/   \  (''") (
)   \   /    \ |  .-.  .-. ;  | |  | |   . .' `. ;   | |  |  .-. ;     | |       |
.-. .   | |   |  .-. .  ;  ,-. ' | |  | |  | |  | |  | |   | '   | |   | |  |  |(___)
| | ___   | |  | |   | |   | |  | |  | |  | | | |  | |  | |  | |  | |   _\_`.(___)  |
|  |  |         | |(   )  | |  | |   | |   | |  | |  | |  | | | |  | |  | |  | |  | |
(   ). '.    | |  |  | ___     | | | |   | |  | |   | |   | |  | |  | |  | | | |  | |
| |  | |  ; '   | |  `\ |   | |  |  '(   )    | ' | |   | |  | |   | |   | |  | |  |
'  | | | |  | |  | |  ' `-'  /   ; '._,' '   | |  '  `-' |     ' `-' ;   | |  | |   |
|   | |  | |  '  `-' |
(___)(___)(___)  '.__.'     '.___.'   (___)  `.__,'       `.__.   (___)(___) (___)
(___)(___)  `.__. | ( `-' ;
                                                                                                `.__.

*/
#ifdef MUSICTHING

#define HAS_OUTPUTS 1
#define HAS_INPUTS 1
#define HAS_CONTROLS 1

#define USEQ_PIN_I1 2
#define USEQ_PIN_I2 3

#define USEQ_PIN_LED_I1 14
#define USEQ_PIN_LED_I2 15

const int useq_output_pins[]     = { 23, 22, 8, 9 };
const int useq_output_led_pins[] = {
    10, 11, 12, 13, USEQ_PIN_LED_I1, USEQ_PIN_LED_I2
};

#define MUX_IN_1 28
#define MUX_IN_2 29

#define MUX_LOGIC_A 24
#define MUX_LOGIC_B 25

#define AUDIO_IN_L 26
#define AUDIO_IN_R 27

#define DAC_SCK 18
#define DAC_SDI 19
#define DAC_CS 21

#define NUM_CONTINUOUS_OUTS 2
#define NUM_BINARY_OUTS 2

#endif

/*


                .--.--.       ,---,.    ,----..                  ,---,       ,----..
               /  /    '.   ,'  .' |   /   /   \              ,`--.' |      /   /   \
         ,--, |  :  /`. / ,---.'   |  /   .     :            /    /  :     /   . :
       ,'_ /| ;  |  |--`  |   |   .' .   /   ;.  \          :    |.' '    .   /   ;.
\
  .--. |  | : |  :  ;_    :   :  |-,.   ;   /  ` ;          `----':  |   .   ;   /  `
;
,'_ /| :  . |  \  \    `. :   |  ;/|;   |  ; \ ; |             '   ' ;   ;   |  ; \ ;
| |  ' | |  . .   `----.   \|   :   .'|   :  | ; | '             |   | |   |   :  | ;
| ' |  | ' |  | |   __ \  \  ||   |  |-,.   |  ' ' ' :             '   : ;   .   |  '
' ' : :  | : ;  ; |  /  /`--'  /'   :  ;/|'   ;  \; /  |             |   | '   '   ;
\; /  | '  :  `--'   \'--'.     / |   |    \ \   \  ',  . \            '   : |
___\   \  ',  / :  ,      .-./  `--'---'  |   :   .'  ;   :      ; |           ; |.'/
.\;   :    /
 `--`----'                |   | ,'     \   \ .'`--"            '---'  \  ; |\   \ .'
                          `----'        `---`                          `--"  `---`


*/

#ifdef USEQHARDWARE_1_0

#define HAS_OUTPUTS 1
#define HAS_INPUTS 1
#define HAS_CONTROLS 1

#define LED_BOARD 6

#define USEQ_PIN_I1 8
#define USEQ_PIN_I2 9

#define USEQ_PIN_AI1 26
#define USEQ_PIN_AI2 27

#define USEQ_PIN_LED_I1 5
#define USEQ_PIN_LED_I2 4

#define USEQ_PIN_LED_AI1 25
#define USEQ_PIN_LED_AI2 24

#define USEQ_PIN_LED_A1 3
#define USEQ_PIN_LED_A2 2
#define USEQ_PIN_LED_A3 11

#define USEQ_PIN_LED_D1 12
#define USEQ_PIN_LED_D2 13
#define USEQ_PIN_LED_D3 22

const int useq_output_pins[]     = { 21, 20, 19, 18, 17, 16 };
const int useq_output_led_pins[] = { USEQ_PIN_LED_A1, USEQ_PIN_LED_A2,
                                     USEQ_PIN_LED_A3, USEQ_PIN_LED_D1,
                                     USEQ_PIN_LED_D2, USEQ_PIN_LED_D3 };

#define USEQ_PIN_SWITCH_M1 10

#define USEQ_PIN_SWITCH_T1 14
#define USEQ_PIN_SWITCH_T2 23

#define NUM_CONTINUOUS_OUTS 3
#define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS)

const String hardwareTypeID = "uSEQ10";  //used to distinguish between hardware on i2c

#define _USEQ_SDA_PIN_ 0
#define _USEQ_SCL_PIN_ 1

#endif

/*


                    _  _  _  _    _  _  _  _  _   _  _  _  _                 _  _ _
   _  _
                  _(_)(_)(_)(_)_ (_)(_)(_)(_)(_)_(_)(_)(_)(_)_            _ (_)(_) _
   _ (_)(_)(_) _ _         _  (_)          (_)(_)           (_)          (_) (_) (_)
   (_)         (_)
   (_)       (_) (_)_  _  _  _   (_) _  _      (_)          (_)         (_) (_) _ (_)
   (_)       (_)   (_)(_)(_)(_)_ (_)(_)(_)     (_)     _    (_)         (_) (_) _ (_)
   (_)       (_)  _           (_)(_)           (_)    (_) _ (_)         (_) (_)  _  _
   _ (_)
   (_)_  _  _(_)_(_)_  _  _  _(_)(_) _  _  _  _(_)_  _  _(_) _           (_) _  _ (_)
   (_)(_)   _ (_) _  _  _
     (_)(_)(_) (_) (_)(_)(_)(_)  (_)(_)(_)(_)(_) (_)(_)(_)  (_)             (_)(_)
   (_)(_)  (_)(_)(_)(_)(_)



*/

///////////////////////////////////////////
#ifdef USEQHARDWARE_0_2

#define HAS_OUTPUTS 1
#define HAS_INPUTS 1
#define HAS_CONTROLS 1

#define LED_BOARD 25

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

const int useq_output_pins[]     = { 21, 20, 19, 18, 17, 16 };
const int useq_output_led_pins[] = { USEQ_PIN_LED_A1, USEQ_PIN_LED_A2,
                                     USEQ_PIN_LED_D1, USEQ_PIN_LED_D2,
                                     USEQ_PIN_LED_D3, USEQ_PIN_LED_D4 };

#define USEQ_PIN_SWITCH_M1 10
#define USEQ_PIN_SWITCH_M2 11

#define USEQ_PIN_SWITCH_T1 14
#define USEQ_PIN_SWITCH_T2 15
#define USEQ_PIN_SWITCH_R1 7

#define USEQ_PIN_ROTARYENC_A 13
#define USEQ_PIN_ROTARYENC_B 12

#define NUM_CONTINUOUS_OUTS 2
#define NUM_BINARY_OUTS (6 - NUM_CONTINUOUS_OUTS)

#endif // USEQHARDWARE_0_2

///////////////////////////////////////////
#ifdef USEQHARDWARE_EXPANDER_OUT_0_1
const String hardwareTypeID = "aout08";  //used to distinguish between hardware on i2c
#define HAS_OUTPUTS 1
#define HAS_INPUTS 0
#define HAS_CONTROLS 0

#define NUM_CONTINUOUS_OUTS 8
#define NUM_BINARY_OUTS 0

#define _USEQ_SDA_PIN_ 4
#define _USEQ_SCL_PIN_ 1


// TODO
const int useq_output_pins[]     = { 13,14,10,11,8,7,5,3};
const int useq_output_led_pins[] = { 15,20,17,12,9,6,2,0};

#endif // USEQHARDWARE_EXPANDER_OUT_0_1

#endif
