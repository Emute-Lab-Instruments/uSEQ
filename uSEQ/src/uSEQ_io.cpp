#include "uSEQ.h"
#include "uSEQ/io_manager.h"
#include "utils.h"
#ifndef ARDUINO
#include "hardware_includes.h"
#endif

// PDM globals moved to IOManager implementation

// Input handling now delegated to IOManager
void uSEQ::handle_input1_interrupt(double ts, int value) {
    // Check for sync trigger
    if (m_waiting_for_sync_trigger && value == 1) {
        m_waiting_for_sync_trigger = false;
        reset_logical_time();
        // TODO propagate the trigger to connected I2C devices
        return;
    }
    
    if (value == 1 && getClockSource() == CLOCK_SOURCES::EXTERNAL_I1) {
        update_clock_from_external(ts);
    }
}

void uSEQ::handle_input2_interrupt(double ts, int value) {
    // Check for sync trigger
    if (m_waiting_for_sync_trigger && value == 1) {
        m_waiting_for_sync_trigger = false;
        reset_logical_time();
        // TODO propagate the trigger to connected I2C devices
        return;
    }
    
    if (value == 1 && getClockSource() == CLOCK_SOURCES::EXTERNAL_I2) {
        update_clock_from_external(ts);
    }
}



// Setup functions moved to IOManager




#if HAS_INPUTS
void uSEQ::update_clock_from_external(double ts)
{
    double newBPM = tempoI1.averageBPM(ts);
    if (ext_clock_tracker.count == 0)
    {
        newBPM *= (4.0 / m_interpreter.get_meter_numerator() / ext_clock_tracker.div);
        // println(String(newBPM));
        // println(String(beatCountI1));
        // println("bar: " + String(barCountI1));
        // println("barpf: " + String(m_bars_per_phrase));
        double std = tempoI1.std();
        // println("std: " + String(std));
        bool highstd = std > 100.0;
        // adjust every bar in high variance, otherwise every phrase
        if ((ext_clock_tracker.beat_count == 0 & highstd) ||
            ext_clock_tracker.beat_count == 0)
        {
            // println("----------------------------------------reset");
            set_bpm(newBPM, 0);
            reset_logical_time();
        }
        ext_clock_tracker.beat_count++;
        if (m_interpreter.get_meter_denominator() == ext_clock_tracker.beat_count)
        {
            ext_clock_tracker.beat_count = 0;
            ext_clock_tracker.bar_count++;
            if (ext_clock_tracker.bar_count ==
                static_cast<size_t>(m_interpreter.get_bars_per_phrase()))
            {
                ext_clock_tracker.bar_count = 0;
            }
        }
    }
    ext_clock_tracker.count++;
    if (ext_clock_tracker.count == ext_clock_tracker.div)
    {
        ext_clock_tracker.count = 0;
        // println("clock=0");
    }
}

// Interrupt handlers and setup functions moved to IOManager
#endif // HAS_INPUTS

// Setup functions moved to IOManager


// Output write functions now delegate to IOManager
void uSEQ::analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val) {
    if (m_io_manager) {
        m_io_manager->analog_write_with_led(output, val);
    }
}

void uSEQ::serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE val) {
    if (m_io_manager) {
        m_io_manager->serial_write(out, val);
    }
}

void uSEQ::digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val) {
    if (m_io_manager) {
        m_io_manager->digital_write_with_led(output, val);
    }
}

void uSEQ::analog_write_led_direct(int pin, CONTINUOUS_OUTPUT_VALUE_TYPE val) {
    if (m_io_manager) {
        m_io_manager->analog_write_led_direct(pin, val);
    }
}

void uSEQ::digital_write_led_direct(int pin, BINARY_OUTPUT_VALUE_TYPE val) {
    if (m_io_manager) {
        m_io_manager->digital_write_led_direct(pin, val);
    }
}



// Filters moved to IOManager

#if HAS_INPUTS
void uSEQ::update_inputs() {
    if (m_io_manager) {
        m_io_manager->update_inputs();
        
#ifdef MUSICTHING
        // Update Music Thing input values as environment variables
        // This allows users to access them directly as 'knob', 'knobx', 'knoby', 'swz'
        // instead of requiring function calls like '(knob)'
        get_environment()->set("knob", Value(m_io_manager->get_input_value(MTMAINKNOB)));
        get_environment()->set("knobx", Value(m_io_manager->get_input_value(MTXKNOB)));
        get_environment()->set("knoby", Value(m_io_manager->get_input_value(MTYKNOB)));
        get_environment()->set("swz", Value(m_io_manager->get_input_value(MTZSWITCH)));
#endif
    }
}
#endif // HAS_INPUTS

// Old update_inputs implementation
#if 0
void uSEQ::update_inputs_OLD()
{
    DBG("uSEQ::update_inputs");

#ifdef USEQHARDWARE_0_2
    read_rotary_encoders();
#endif

    // inputs are input_pullup, so invert
    // auto now              = micros();
    const double recp4096 = 0.000244141; // 1/4096
    const double recp2048 = 1 / 2048.0;
    const double recp1024 = 1 / 1024.0;

#ifdef MUSICTHING
    const size_t muxdelay = 2;

    // unroll loop for efficiency
    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTMAINKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI1]    = 1.0 - (analogRead(MUX_IN_2) * recp4096);

    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 1);
    delayMicroseconds(muxdelay);
    m_input_vals[MTYKNOB] = analogRead(MUX_IN_1) * recp4096;

    digitalWrite(MUX_LOGIC_A, 1);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTXKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI2] = 1.0 - (analogRead(MUX_IN_2) * recp4096);

    digitalWrite(MUX_LOGIC_A, 1);
    digitalWrite(MUX_LOGIC_B, 1);
    delayMicroseconds(muxdelay);
    int switchVal = analogRead(MUX_IN_1);
    if (switchVal < 100)
    {
        switchVal = 0;
    }
    else if (switchVal > 3500)
    {
        switchVal = 2;
    }
    else
    {
        switchVal = 1;
    }
    m_input_vals[MTZSWITCH] = switchVal;

    // println(m_input_vals[MTMAINKNOB]);
    // println("\t");
    // println(m_input_vals[MTXKNOB]);
    // println("\t");
    // println(m_input_vals[MTYKNOB]);
    // println("\t");
    // println(m_input_vals[MTZSWITCH]);

    // const int input1 = 1 - digitalRead(USEQ_PIN_I1);
    // const int input2 = 1 - digitalRead(USEQ_PIN_I2);
    // digitalWrite(useq_output_led_pins[4], input1);
    // digitalWrite(useq_output_led_pins[5], input2);
    // m_input_vals[USEQI1] = input1;
    // m_input_vals[USEQI2] = input2;

#else

    // #ifdef USEQHARDWARE_1_0
    // #else
    m_input_vals[USEQT1] = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
#endif // MUSICTHING

#ifdef USEQHARDWARE_0_2
    m_input_vals[USEQRS1] = 1 - digitalRead(USEQ_PIN_SWITCH_R1);
    m_input_vals[USEQM2]  = 1 - digitalRead(USEQ_PIN_SWITCH_M2);
    m_input_vals[USEQT2]  = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
#endif

#ifdef USEQHARDWARE_1_0
    // TOGGLES
    const int ts_a = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
    const int ts_b = 1 - digitalRead(USEQ_PIN_SWITCH_T2);

    if ((ts_a == 0) && (ts_b == 0))
    {
        m_input_vals[USEQT1] = 1;
    }
    else
    {
        if (ts_a == 1)
        {
            m_input_vals[USEQT1] = 2;
        }
        else
        {
            m_input_vals[USEQT1] = 0;
        }
    }

    // MOMENTARY
    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);

    // switch off LED while making measurements
    //  digitalWrite(USEQ_PIN_LED_AI1, 0);
    //  digitalWrite(USEQ_PIN_LED_AI2, 0);
    //  delayMicroseconds(100);

    // pdm_w = 0;
    // delayMicroseconds(10);

    auto v_ai1 = analogRead(USEQ_PIN_AI1);
    // <<<<<<< HEAD
    //     auto v_ai1_11 = v_ai1;                       // scale from 10 bit to 11
    //     bit range v_ai1_11      = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp
    //     curve analogWrite(USEQ_PIN_LED_AI1, v_ai1_11);

    //     auto v_ai2    = analogRead(USEQ_PIN_AI2);
    //     auto v_ai2_11 = v_ai2;
    //     v_ai2_11      = (v_ai2_11 * v_ai2_11) >> 11;
    //     analogWrite(USEQ_PIN_LED_AI2, v_ai2_11);

    //     const double lpcoeff  = 0.05; // 0.009;
    //     m_input_vals[USEQAI1] = cvInFilter[0].lopass(v_ai1 * recp2048, lpcoeff);
    //     m_input_vals[USEQAI2] = cvInFilter[1].lopass(v_ai2 * recp2048, lpcoeff);
    // =======
    auto v_ai2 = analogRead(USEQ_PIN_AI2);

    auto v_ai1_11 = v_ai1;
    v_ai1_11      = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp curve

    // analogWriteFreq(25000);    // out of hearing range

    // digitalWrite(USEQ_PIN_LED_AI2, 0);

    // analogWrite(USEQ_PIN_LED_AI1, v_ai1_11 + random(-100,100));
    pdm_w         = v_ai1_11 / 2048.0;
    auto v_ai2_11 = v_ai2;

    v_ai2_11 = (v_ai2_11 * v_ai2_11) >> 11;
    analogWrite(USEQ_PIN_LED_AI2, v_ai2_11);

    const double lpcoeff = 0.2; // 0.009;
    double filt1         = mf1.process(v_ai1 * recp2048);
    double filt2         = mf2.process(v_ai2 * recp2048);
    // m_input_vals[USEQAI1] = cvInFilter[0].lopass(v_ai1 * recp2048, lpcoeff);
    // m_input_vals[USEQAI2] = cvInFilter[1].lopass(v_ai2 * recp2048, lpcoeff);
    m_input_vals[USEQAI1] = filt1;
    m_input_vals[USEQAI2] = filt2;
#endif

    dbg("updating inputs...DONE");
}
#endif // HAS_INPUTS


#ifdef ARDUINO

Value uSEQ::useq_swm(std::vector<Value> &args,
                                    Environment &env) {
    constexpr const char *user_facing_name = "swm";

    // Checking number of args
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    if (m_io_manager) {
        result = Value(m_io_manager->get_input_value(USEQM1));
    }
    return result;
}

Value uSEQ::useq_swt(std::vector<Value> &args,
                                    Environment &env) {
    constexpr const char *user_facing_name = "swt";

    // Checking number of args
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    if (m_io_manager) {
        result = Value(m_io_manager->get_input_value(USEQT1));
    }
    return result;
}


Value uSEQ::useq_ssin(std::vector<Value> &args,
                                     Environment &env) {
    constexpr const char *user_facing_name = "ssin";

    // Checking number of args
    if (!(args.size() == 1)) {
        report_error_wrong_num_args(user_facing_name,
                                    static_cast<int>(args.size()),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; static_cast<size_t>(i) < args.size(); i++) {
        // Eval
        Value pre_eval = args[i];
        args[i] = args[i].eval(env);
        if (args[i].is_error()) {
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    int index = args[0].as_int();
    Value result = Value::nil();
    if (index > 0 && index <= m_num_serial_ins) {
        result = Value(m_serial_input_streams[index - 1]);
    } else {
        report_user_warning("(ssin) Received request for index " +
                            String(index) +
                            ", which is out of bounds; returning nil.");
    }

    return result;
}

// Pin mapping functions moved to IOManager
#endif

// Rotary encoder functions moved to IOManager

// PDM functions moved to IOManager



// PIO PWM functions moved to IOManager


// Timer callback moved to IOManager





// MIDI OUT

#ifdef MIDIOUT
double last_midi_t = 0;
void uSEQ::update_midi_out()
{
    DBG("uSEQ::update_midi_out");
    const double midiRes        = 48 * meter_numerator * 1;
    // FIXME: where is this barDur supposed to be coming from?
    const double timeUnitMillis = (barDur / midiRes);

    const double timeDeltaMillis = t - last_midi_t;
    size_t steps                 = floor(timeDeltaMillis / timeUnitMillis);
    double initValPhase          = bar - (timeDeltaMillis / barDur);

    if (steps > 0)
    {
        const double timeUnitBar = 1.0 / midiRes;

        auto itr = useqMDOMap.begin();
        for (; itr != useqMDOMap.end(); itr++)
        {
            // Iterate through the keys process MIDI events
            Value midiFunction = itr->second;
            if (initValPhase < 0)
                initValPhase++;
            std::vector<Value> mdoArgs = { Value(initValPhase) };
            Value prev                 = midiFunction.apply(mdoArgs, env);
            for (size_t step = 0; step < steps; step++)
            {
                double t_step = bar - ((steps - (step + 1)) * timeUnitBar);
                // wrap phasor
                if (t_step < 0)
                    t_step += 1.0;
                // println(t_step);
                mdoArgs[0] = Value(t_step);
                Value val  = midiFunction.apply(mdoArgs, env);

                // println(val.as_float());
                if (val > prev)
                {
                    Serial1.write(0x99);
                    Serial1.write(itr->first);
                    Serial1.write(val.as_int() * 14);
                }
                else if (val < prev)
                {
                    Serial1.write(0x89);
                    Serial1.write(itr->first);
                    Serial1.write((byte)0);
                }
                prev = val;
            }
        }
        last_midi_t = t;
    }
}


#endif // end of MIDI OUT SECTION
