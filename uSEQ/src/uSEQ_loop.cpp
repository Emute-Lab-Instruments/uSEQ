#include "uSEQ.h"



void uSEQ::run()
{
    if (!m_initialised)
    {
        init();
    }

    start_loop_blocking();
}

void uSEQ::start_loop_blocking()
{
    while (!m_should_quit)
    {
        tick();
    }

    println("Exiting REPL.");
}

void uSEQ::tick()
{
    DBG("uSEQ::tick");

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