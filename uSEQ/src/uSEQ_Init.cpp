#include "uSEQ_Init.h"
#include "uSEQ.h"
#include "uSEQ_LispFunctions.h"
#include "uSEQ_IO.h"
#include "uSEQ_HardwareControl.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_INIT_MODULE
// These functions are defined when the module is enabled
void setup_leds()
{
#if USEQ_DEBUG
    debug("uSEQ::setup_leds");
#endif

#ifndef MUSICTHING
    pinMode(LED_BOARD, OUTPUT);
    digitalWrite(LED_BOARD, 1);
#endif

#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_LED_AI1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_AI2, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I2, OUTPUT_2MA);
#endif

    for (int i = 0; i < 6; i++)
    {
        pinMode(useq_output_led_pins[i], OUTPUT_2MA);
        gpio_set_slew_rate(useq_output_led_pins[i], GPIO_SLEW_RATE_SLOW);
    }
}

#ifdef USEQHARDWARE_1_0
// Implementation of setup_pdm() function
void uSEQ::setup_pdm()
{
#if USEQ_DEBUG
    debug("uSEQ::setup_pdm");
#endif
    
    // PDM setup code 
    // (Will be filled in during full refactoring)
}
#endif

void uSEQ::init()
{
#if USEQ_DEBUG
    debug("uSEQ::init");
#endif

    init_hardware();
    init_variables();
    init_ASTs();
    init_builtinfuncs();
    eval_lisp_library();
    
    m_initialised = true;
}

void uSEQ::init_hardware()
{
    setup_leds();
    setup_IO();
    
#ifdef USEQHARDWARE_1_0
    setup_pdm();
    start_pdm();
#endif
}

void uSEQ::init_variables()
{
    m_bpm = DEFAULT_BPM;
    m_nominal_bpm = DEFAULT_BPM;
    //update_bpm_variables();
    
    //reset_logical_time();
    
    m_should_quit = false;
    m_initialised = false;
    m_external_clock_source = false;
    m_encoder_delta = 0;
    
    // Initialize ASTs and values
    m_continuous_ASTs.resize(NUM_CONTINUOUS_OUTS);
    m_continuous_vals.resize(NUM_CONTINUOUS_OUTS, 0);
    
    m_binary_ASTs.resize(NUM_BINARY_OUTS);
    m_binary_vals.resize(NUM_BINARY_OUTS, 0);
    
    m_serial_ASTs.resize(NUM_SERIAL_OUTS);
    m_serial_vals.resize(NUM_SERIAL_OUTS, std::nullopt);
    
    // Initialize output structures (transitional)
    for (int i = 0; i < NUM_CONTINUOUS_OUTS; i++)
    {
        m_continuous_outputs[i].active = false;
        m_continuous_outputs[i].current_value = 0;
    }
    
    for (int i = 0; i < NUM_BINARY_OUTS; i++)
    {
        m_binary_outputs[i].active = false;
        m_binary_outputs[i].current_value = false;
    }
    
    for (int i = 0; i < NUM_SERIAL_OUTS; i++)
    {
        m_serial_outputs[i].active = false;
        m_serial_outputs[i].current_value = 0; // Use numeric values for now
    }
}

void uSEQ::led_animation()
{
    // LED startup animation
    for (int i = 0; i < 6; i++)
    {
        digitalWrite(useq_output_led_pins[i], HIGH);
        delay(100);
        digitalWrite(useq_output_led_pins[i], LOW);
    }
    
    for (int i = 5; i >= 0; i--)
    {
        digitalWrite(useq_output_led_pins[i], HIGH);
        delay(100);
        digitalWrite(useq_output_led_pins[i], LOW);
    }
}

void uSEQ::init_ASTs()
{
    // Initialize AST data structures
    m_ast_pool.init();
    m_ast_allocator.init(&m_ast_pool);
    
    // Initialize with default expressions
    m_q0AST = parse("0"); // Default Q0 expression
    
    if (default_continuous_expr.is_nil())
        default_continuous_expr = parse("0");
        
    if (default_binary_expr.is_nil())
        default_binary_expr = parse("0");
        
    if (default_serial_expr.is_nil())
        default_serial_expr = parse("0");
        
    // Initialize ASTs with default values
    for (int i = 0; i < m_continuous_ASTs.size(); i++)
        m_continuous_ASTs[i] = default_continuous_expr;
        
    for (int i = 0; i < m_binary_ASTs.size(); i++)
        m_binary_ASTs[i] = default_binary_expr;
        
    for (int i = 0; i < m_serial_ASTs.size(); i++)
        m_serial_ASTs[i] = default_serial_expr;
}

void uSEQ::eval_lisp_library()
{
    // Load and evaluate standard library functions
#ifdef USEQ_STANDARD_LIB
    eval_stream(USEQ_STANDARD_LIB, env);
#endif
}

#endif // USE_NEW_MODULES && USE_INIT_MODULE