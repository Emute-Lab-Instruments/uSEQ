#include "uSEQ.h"
#include "uSEQ_Init.h"
#include "uSEQ_Timing.h"
#include "uSEQ_IO.h"
#include "uSEQ_Signals.h"
#include "uSEQ_Scheduler.h"
#include "uSEQ_Storage.h"
#include "uSEQ_LispFunctions.h"
#include "uSEQ_HardwareControl.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_CORE_MODULE
// Initialize the static instance
uSEQ* uSEQ::instance;

// Core entry point and event loop implementations

// Implementation of the main run method - kept in the core file
void uSEQ::run()
{
    if (!m_initialised)
    {
        init();
    }

    start_loop_blocking();
}

// Implementation of the main loop - kept in the core file
void uSEQ::start_loop_blocking()
{
    while (!m_should_quit)
    {
        tick();
    }

    println("Exiting REPL.");
}

// Implementation of the tick method - kept in the core file 
void uSEQ::tick()
{
#if USEQ_DEBUG
    debug("uSEQ::tick");
#endif

    updateSpeed = micros() - ts;
    set("fps", Value(1000000.0 / updateSpeed));
    set("qt", Value(updateSpeed * 0.001));
    ts = micros();
    
    // Read & cache the hardware & software inputs
    update_inputs();
    
    // Update time
    update_time();
    
    check_code_quant_phasor();
    run_scheduled_items();
    
    // Re-run & cache output signal forms
    update_signals();
    
    // Write cached output signals to hardware and/or software outputs
    update_outs();
    
    // Check for new code and eval (or schedule it)
    check_and_handle_user_input();

    // tiny delay to allow for interrupts etc
    delayMicroseconds(100);
}
#endif // USE_NEW_MODULES && USE_CORE_MODULE