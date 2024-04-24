#include "uSEQ.h"
#include "lisp/interpreter.h"
#include "lisp/value.h"
#include "utils.h"
#include "utils/log.h"
// #include "lisp/library.h"
#include <cmath>

// uSEQ MEMBER FUNCTIONS

#define BUILTINFUNC_MEMBER(__name__, __body__, __numArgs__)                                                  \
    Value uSEQ::__name__(std::vector<Value>& args, Environment& env)                                         \
    {                                                                                                        \
        eval_args(args, env);                                                                                \
        Value ret = Value();                                                                                 \
        if (args.size() != __numArgs__)                                                                      \
        {                                                                                                    \
            println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                               \
            ret = Value::error();                                                                            \
        }                                                                                                    \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

#define BUILTINFUNC_VARGS_MEMBER(__name__, __body__, __minArgs__, __maxArgs__)                               \
    Value uSEQ::__name__(std::vector<Value>& args, Environment& env)                                         \
    {                                                                                                        \
        eval_args(args, env);                                                                                \
        Value ret = Value();                                                                                 \
        if (args.size() < __minArgs__ || args.size() > __maxArgs__)                                          \
            println(args.size() > __maxArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                               \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

#define BUILTINFUNC_NOEVAL_MEMBER(__name__, __body__, __numArgs__)                                           \
    Value uSEQ::__name__(std::vector<Value>& args, Environment& env)                                         \
    {                                                                                                        \
        Value ret = Value();                                                                                 \
        if (args.size() != __numArgs__)                                                                      \
            println(args.size() > __numArgs__ ? TOO_MANY_ARGS : TOO_FEW_ARGS);                               \
        else                                                                                                 \
        {                                                                                                    \
            __body__                                                                                         \
        }                                                                                                    \
        return ret;                                                                                          \
    }

// void dbg(String s) { std::cout << s.c_str() << std::endl; }

String exit_command = "@@exit";

void uSEQ::run()
{
    dbg("Hello!");
    if (!m_initialised)
    {
        init();
    }

    dbg("Starting loop");
    start_loop_blocking();
}

// void uSEQ::init_inputs()
// {
//     for (int i = 0; i < m_num_binary_ins; i++)
//     {
//         m_inputs.push_back(Input(INPUT_BINARY, i));
//     }
// }

// void uSEQ::init_outputs()
// {
//     // BINARY
//     for (int i = 0; i < m_num_binary_outs; i++)
//     {
//         m_outputs.push_back(Output(OUTPUT_BINARY, i));
//     }

//     // CONTINUOUS
//     for (int i = 0; i < m_num_continuous_outs; i++)
//     {
//         m_outputs.push_back(Output(INPUT_CONTINUOUS, i));
//     }
// }

void uSEQ::init()
{
    m_io.init();
    insert_own_builtinfuncs();
    setBpm(m_defaultBPM, 0);
    initASTs();
    update_time();
    m_initialised = true;
}

void uSEQ::start_loop_blocking()
{
    while (!m_should_quit)
    {
        tick();
    }

    println("Exiting REPL.");
}

// TODO does order matter?
// e.g. when user code is evaluated, does it make
// a difference if the inputs have been updated already?
void uSEQ::tick()
{
    // TODO remove comment
    // dbg("ticking");
    // Read & cache the hardware & software inputs
    update_inputs();
    // Update time
    update_time();
    // Re-run & cache output signal forms
    update_signals();
    // Write cached output signals to hardware & software outputs
    update_outputs();
    // Check for new code and eval
    check_and_handle_user_input();
}

void uSEQ::update_inputs()
{
    // dbg("updating inputs!");
}

void uSEQ::check_and_handle_user_input()
{
    // dbg("handling inputs!");
    // m_repl.check_and_handle_input();

    if (m_io.is_new_code_waiting())
    {
        m_last_received_code = m_io.get_latest_code();

        if (m_last_received_code == exit_command)
        {
            m_should_quit = true;
        }
        else
        {
            try
            {
                String result = eval(m_last_received_code);
                print("==> ");
                print(result);
                print("\n");
            }
            catch (const std::exception& e)
            {
                // error("Error: " + e.what());
                error("Error handling user input: ");
            }

            print(">> ");
        }
    }
}

void uSEQ::update_continuous_signals()
{
    for (int i = 0; i < m_num_continuous_outs; i++)
    {
        Value v = m_interpreter.eval(m_continuous_ASTs[i]);
        if (v == Value::error() /*|| !currentExprSound*/)
        {
            error("Error in analog output function, clearing");
            m_continuous_ASTs[i] = { default_continuous_form };
            // currentExprSound     = true;
            m_continuous_vals[i] = 0;
        }
        else
        {
            double n             = v.as_float();
            m_continuous_vals[i] = n;
        }
    }
}

void uSEQ::update_binary_signals()
{
    for (int i = 0; i < m_num_binary_outs; i++)
    {
        Value v          = m_interpreter.eval(m_continuous_ASTs[i]);
        int n            = v.as_int();
        m_binary_vals[i] = n;
    }
}

// TODO
// update_misc_signals();

void uSEQ::update_signals()
{
    dbg("updating signals!");
    update_continuous_signals();
    update_binary_signals();
    // update_misc_signals();
}

void uSEQ::update_outputs()
{
    update_continuous_outs();
    update_binary_outs();
}

void uSEQ::update_continuous_outs()
{
#if defined(USE_ARDUINO_PIO)
    const double maxpwm = 8191.0;
    for (int i = 0; i < m_num_continuous_outs; i++)
    {
        int sigval v = m_continuous_vals[i] * maxpwm;
        // Clamp
        sigval = std::min(sigval, maxpwm);
        m_io.analog_write_with_led(i, v);
    }
#endif
}

void uSEQ::update_binary_outs() {}

// Toplevel
String uSEQ::eval(const String& code) { return m_interpreter.eval(code); }

/// UPDATE methods
void uSEQ::setTime(size_t newTimeMicros)
{
    time = newTimeMicros;
    // last_t = t;
    t       = newTimeMicros - lastResetTime;
    beat    = fmod(t, beatDur) / beatDur;
    bar     = fmod(t, barDur) / barDur;
    phrase  = fmod(t, phraseDur) / phraseDur;
    section = fmod(t, sectionDur) / sectionDur;
    updateTimeVariables();
}

void uSEQ::updateTimeVariables()
{
    set("time", Value(time));
    set("t", Value(t));

    // phasors
    set("beat", Value(beat));
    set("bar", Value(bar));
    set("phrase", Value(phrase));
    set("section", Value(section));
}

void uSEQ::update_time() { setTime(micros()); }

void uSEQ::updateBpmVariables()
{
    set("bpm", Value(bpm));
    // set("bps", Value(bps));
    // set("beatDur", Value(beatDur));
    // set("barDur", Value(barDur));
    // set("phraseDur", Value(phraseDur));
    // set("sectionDur", Value(sectionDur));
}

// void uSEQ::updateDigitalOutputs()
// {
//     //     for (size_t i = 0; i < BINARY_OUTS; i++)
//     //     {
//     //         currentExprSound = true;
//     //         Value result     = runParsedCode(m_binary_ASTs[i], env);

//     //         if (result == Value::error() || !currentExprSound)
//     //         {
//     //             println("Error in digital output function, clearing");
//     //             m_binary_ASTs[i] = { defaultForm_digital };
//     //             currentExprSound = true;
//     //         }
//     //         // Write
//     //         else
//     //         {
//     //             int pin     = digital_out_pin(i + 1);
//     //             int led_pin = digital_out_LED_pin(i + 1);
//     //             int val     = result.as_int();
//     //             // TODO: this is repeat of ard_useqdw, should rationalise
//     // #ifdef DIGI_OUT_INVERT
//     //             digitalWrite(pin, 1 - val);
//     // #else
//     //             digitalWrite(pin, val);
//     // #endif
//     //             digitalWrite(led_pin, val);
//     //         }
//     //     }
// }

// void uSEQ::updateAnalogOutputs()
// {
//     // for (const Value& form : m_continuous_ASTs)
//     // {
//     //     // currentExprSound = true;
//     //     Value result =

//     //         if (result == Value::error() || !currentExprSound)
//     //     {
//     //         println("Error in analog output function, clearing");
//     //         analogASTs[i]    = { defaultForm_analog };
//     //         currentExprSound = true;
//     //     }
//     //     // Write
//     //     else
//     //     {
//     //         // PWM out
//     //         const double maxpwm = 8191.0;
//     //         int sigval          = result.as_float() * maxpwm;
//     //         if (sigval > maxpwm)
//     //             sigval = maxpwm;
//     //         pio_pwm_set_level(i < 4 ? pio0 : pio1, i % 4, sigval);

//     //         // led out
//     //         int led_pin   = analog_out_LED_pin(i + 1);
//     //         int ledsigval = sigval >> 2; // shift to 11 bit range for the LED
//     //         ledsigval     = (ledsigval * ledsigval) >> 11; // cheap way to square and get a exp curve
//     //         analogWrite(led_pin, ledsigval);
//     //     }
//     // }
// }

void uSEQ::updateQ0()
{
    // currentExprSound = true;
    // Value result     = runParsedCode(q0AST, env);

    // if (result == Value::error() || !currentExprSound)
    // {
    //     println("Error in q0 output function, clearing");
    //     q0AST            = {};
    //     currentExprSound = true;
    // }
}

// void uSEQ::updateMiscOutputs()
// {
//     // for (size_t i = 0; i < SERIAL_OUTS; i++)
//     // {
//     //     if (serialASTs[i].size() > 0)
//     //     {
//     //         currentExprSound = true;
//     //         Value result     = runParsedCode(serialASTs[i], env);

//     //         if (result == Value::error() || !currentExprSound)
//     //         {
//     //             println("Error in serial output function, clearing");
//     //             serialASTs[i]    = {};
//     //             currentExprSound = true;
//     //         }
//     //         // Write
//     //         else
//     //         {
//     //             double val = result.as_float();
//     //             write((u_int8_t)31);
//     //             write((u_int8_t)(i + 1));
//     //             char* byteArray = reinterpret_cast<char*>(&val);
//     //             for (size_t b = 0; b < 8; b++)
//     //             {
//     //                 write(byteArray[b]);
//     //             }
//     //         }
//     //     }
//     // }
// }

void uSEQ::set(String name, Value val) { m_interpreter.set(name, val); }

void uSEQ::initASTs()
{
    for (int i = 0; i < m_num_binary_outs; i++)
    {
        m_binary_ASTs.push_back(default_binary_form);
        m_binary_vals.push_back(0);
    }

    for (int i = 0; i < m_num_continuous_outs; i++)
    {
        m_continuous_ASTs.push_back(default_continuous_form);
        m_continuous_vals.push_back(0.0);
    }

    for (int i = 0; i < m_num_misc_outs; i++)
    {
        // TODO
        // m_continuous_ASTs.push_back(default_continuous_form);
        // m_continuous_vals.push_back(0.0);
    }
}

void uSEQ::setBpm(double newBpm, double changeThreshold = 0.0)
{
    if (fabs(newBpm - bpm) >= changeThreshold)
    {
        bpm = newBpm;
        bps = bpm / 60.0;

        beatDur    = 1000000.0 / bps;
        barDur     = beatDur * (4.0 / meter_denominator) * meter_numerator;
        phraseDur  = barDur * barsPerPhrase;
        sectionDur = phraseDur * phrasesPerSection;

        updateBpmVariables();
    }
}

#if defined(USE_ARDUINO_PIO)
bool analog_write_with_led(int pin, int led_pin, int val)
{
    analogWrite(pin, val);
    // TODO this should be different val?
    analogWrite(led_pin, val);

    // TODO return correct bool, e.g. to indicate errors with writing
    return true;
}
#endif

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, set("q-form", args[0]); m_q0AST = { args[0] };, 1)

BUILTINFUNC_NOEVAL_MEMBER(
    a1,
    if (NUM_CONTINUOUS_OUTS >= 1) {
        set("a1-form", args[0]);
        m_continuous_ASTs[0] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    a2,
    if (NUM_CONTINUOUS_OUTS >= 2) {
        set("a2-form", args[0]);
        m_continuous_ASTs[1] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    a3,
    if (NUM_CONTINUOUS_OUTS >= 3) {
        set("a3-form", args[0]);
        m_continuous_ASTs[2] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    a4,
    if (NUM_CONTINUOUS_OUTS >= 4) {
        set("a4-form", args[0]);
        m_continuous_ASTs[3] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    a5,
    if (NUM_CONTINUOUS_OUTS >= 5) {
        set("a5-form", args[0]);
        m_continuous_ASTs[4] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    a6,
    if (NUM_CONTINUOUS_OUTS >= 6) {
        set("a6-form", args[0]);
        m_continuous_ASTs[5] = { args[0] };
    },
    1)

// DIGITAL OUTS
BUILTINFUNC_NOEVAL_MEMBER(
    d1,
    if (NUM_BINARY_OUTS >= 1) {
        set("d1-form", args[0]);
        m_binary_ASTs[0] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    d2,
    if (NUM_BINARY_OUTS >= 2) {
        set("d2-form", args[0]);
        m_binary_ASTs[1] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    d3,
    if (NUM_BINARY_OUTS >= 3) {
        set("d3-form", args[0]);
        m_binary_ASTs[2] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    d4,
    if (NUM_BINARY_OUTS >= 4) {
        set("d4-form", args[0]);
        m_binary_ASTs[3] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    d5,
    if (NUM_BINARY_OUTS >= 5) {
        set("d5-form", args[0]);
        m_binary_ASTs[4] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    d6,
    if (NUM_BINARY_OUTS >= 6) {
        set("d6-form", args[0]);
        m_binary_ASTs[5] = { args[0] };
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(s1, set("s1-form", args[0]); m_misc_ASTs[0] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s2, set("s2-form", args[0]); m_misc_ASTs[1] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s3, set("s3-form", args[0]); m_misc_ASTs[2] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s4, set("s4-form", args[0]); m_misc_ASTs[3] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s5, set("s5-form", args[0]); m_misc_ASTs[4] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s6, set("s6-form", args[0]); m_misc_ASTs[5] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s7, set("s7-form", args[0]); m_misc_ASTs[6] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(s8, set("s8-form", args[0]); m_misc_ASTs[7] = { args[0] };, 1)

double fast(double speed, double phasor)
{
    phasor *= speed;
    double phase = fmod(phasor, 1.0);
    return phase;
}

BUILTINFUNC_MEMBER(lisp_fast, double speed = args[0].as_float(); double phasor = args[1].as_float();
                   double fastPhasor = fast(speed, phasor); ret = Value(fastPhasor);, 2)

BUILTINFUNC_NOEVAL_MEMBER(lisp_slow, double factor = args[0].eval(env).as_float(); Value expr = args[1];
                          // store the current time to reset later
                          double currentTime = env.get("time").as_float();
                          // update the interpreter's time just for this expr
                          double newTime                               = currentTime / factor;
                          setTime((size_t)newTime); double evaled_expr = expr.eval(env).as_float();
                          ret                                          = Value(evaled_expr);
                          // restore the interpreter's time
                          setTime((size_t)currentTime);, 2)

// (schedule <name> <statement> <period>)
BUILTINFUNC_NOEVAL_MEMBER(lisp_schedule, const auto itemName = args[0].as_string(); const auto ast = args[1];
                          const auto period = args[2].as_float(); scheduledItem v; v.id = itemName;
                          v.period = period; v.lastRun = 0;
                          //    v.statement = statement;
                          v.ast                               = { ast };
                          m_scheduled_items.push_back(v); ret = Value(0);, 3)

BUILTINFUNC_MEMBER(
    lisp_unschedule, const String id = args[0].as_string();
    auto is_item = [id](scheduledItem& v) { return v.id == id; };

    if (auto it = std::find_if(begin(m_scheduled_items), end(m_scheduled_items), is_item);
        it != std::end(m_scheduled_items)) {
        m_scheduled_items.erase(it);
        println("Item removed");
    } else { println("Item not found"); },
    1)

BUILTINFUNC_VARGS_MEMBER(useq_setbpm, double newBpm = args[0].as_float();
                         double thresh = args.size() == 2 ? args[1].as_float() : 0; setBpm(newBpm, thresh);
                         ret           = args[0];, 1, 2)

BUILTINFUNC_MEMBER(
    useq_getbpm, int index = args[0].as_int(); if (index == 1) {
        ret = tempoI1.avgBPM;
    } else if (index == 1) { ret = tempoI2.avgBPM; } else { ret = 0; },
                                               1)

BUILTINFUNC_MEMBER(useq_settimesig, setTimeSignature(args[0].as_float(), args[1].as_float()); ret = Value(1);
                   , 2)

BUILTINFUNC_MEMBER(useq_in1, ret = Value(m_input_vals[USEQI1]);, 0)
BUILTINFUNC_MEMBER(useq_in2, ret = Value(m_input_vals[USEQI2]);, 0)
BUILTINFUNC_MEMBER(useq_ain1, ret = Value(m_input_vals[USEQAI1]);, 0)
BUILTINFUNC_MEMBER(useq_ain2, ret = Value(m_input_vals[USEQAI2]);, 0)

#ifdef MUSICTHING
BUILTINFUNC_MEMBER(useq_mt_knob, ret = Value(m_input_vals[MTMAINKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_knobx, ret = Value(m_input_vals[MTXKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_knoby, ret = Value(m_input_vals[MTYKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_swz, ret = Value(m_input_vals[MTZSWITCH]);, 0)
#endif

BUILTINFUNC_MEMBER(
    useq_ssin, int index = args[0].as_int();
    if (index > 0 && index <= NUM_MISC_INS) { ret = Value(m_serialInputStreams[index - 1]); }, 1)

BUILTINFUNC_MEMBER(
    useq_swm, int index = args[0].as_int();
    if (index == 1) { ret = Value(m_input_vals[USEQM1]); } else { ret = Value(m_input_vals[USEQM2]); }, 1)

BUILTINFUNC_MEMBER(
    useq_swt, int index = args[0].as_int();
    if (index == 1) { ret = Value(m_input_vals[USEQT1]); } else { ret = Value(m_input_vals[USEQT2]); }, 1)

BUILTINFUNC_MEMBER(useq_swr, ret = Value(m_input_vals[USEQRS1]);, 0)

BUILTINFUNC_MEMBER(useq_rot, ret = Value(m_input_vals[USEQR1]);, 0)

//(drum-predict <input-pattern>) -> list
// BUILTINFUNC_MEMBER(
//     useq_drumpredict, const std::vector<Value> inputs = args[0].as_list(); std::vector<char> invec(32, 1);
//     for (size_t i = 0; i < 32; i++) { invec[i] = inputs[i].as_int(); } std::vector<int> outvec(14, 0);
//     apply_logic_gate_net_singleval(invec.data(), outvec.data()); std::vector<Value> result(14);
//     for (size_t i = 0; i < 14; i++) { result.at(i) = Value(outvec.at(i)); } ret = Value(result);, 1)

Value fromList(std::vector<Value>& lst, double phasor, Environment& env)
{
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1.0)
    {
        phasor = 1.0;
    }
    double scaled_phasor = lst.size() * phasor;
    size_t idx           = floor(scaled_phasor);
    if (idx == lst.size())
        idx--;
    return lst[idx].eval(env);
}

BUILTINFUNC_MEMBER(useq_dm, auto index = args[0].as_int(); auto v1 = args[1].as_float();
                   auto v2 = args[2].as_float(); ret = Value(index > 0 ? v2 : v1);, 3)

BUILTINFUNC_VARGS_MEMBER(useq_gates, auto lst = args[0].as_list(); const double phasor = args[1].as_float();
                         const double speed      = args[2].as_float();
                         const double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.5;
                         const double val        = fromList(lst, fast(speed, phasor), env).as_int();
                         const double gates      = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
                         ret                     = Value(val * gates);, 3, 4)

BUILTINFUNC_VARGS_MEMBER(useq_gatesw, auto lst = args[0].as_list(); const double phasor = args[1].as_float();
                         const double speed      = args.size() == 3 ? args[2].as_float() : 1.0;
                         const double val        = fromList(lst, fast(speed, phasor), env).as_int();
                         const double pulseWidth = val / 9.0;
                         const double gate       = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
                         ret                     = Value((val > 0 ? 1.0 : 0.0) * gate);, 2, 3)

BUILTINFUNC_VARGS_MEMBER(useq_trigs, auto lst = args[0].as_list(); const double phasor = args[1].as_float();
                         const double speed      = args.size() == 3 ? args[2].as_float() : 1.0;
                         const double val        = fromList(lst, fast(speed, phasor), env).as_int();
                         const double amp        = val / 9.0;
                         const double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.1;
                         const double gate       = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
                         ret                     = Value((val > 0 ? 1.0 : 0.0) * gate * amp);, 2, 4)

BUILTINFUNC_MEMBER(useq_loopPhasor,
                   auto phasor    = args[0].as_float();
                   auto loopPoint = args[1].as_float(); if (loopPoint == 0) loopPoint = 1; // avoid infinity
                   double spedupPhasor = fast(1.0 / loopPoint, phasor); ret = spedupPhasor * loopPoint;, 2)

// (euclid <phasor> <n> <k> (<offset>) (<pulsewidth>)
BUILTINFUNC_VARGS_MEMBER(
    useq_euclidean, const double phasor = args[0].as_float(); const int n = args[1].as_int();
    const int k = args[2].as_int(); const int offset = (args.size() >= 4) ? args[3].as_int() : 0;
    const float pulseWidth = (args.size() == 5) ? args[4].as_float() : 0.5; const float fi = phasor * n;
    int i = static_cast<int>(fi); const float rem = fi - i;
    if (i == n) { i--; } const int idx            = ((i + n - offset) * k) % n;
    ret                                           = Value(idx < k && rem < pulseWidth ? 1 : 0);, 3, 5)

// (step <phasor> <count> (<offset>))

//////////////
BUILTINFUNC_VARGS_MEMBER(
    useq_fromList, auto lst = args[0].as_list(); const double phasor = args[1].as_float();
    ret = fromList(lst, phasor, env); if (args.size() == 3) {
        double scale = args[2].as_float();
        if (scale != 0)
        {
            ret = Value(ret / scale);
        }
    },
                                      2, 3)

Value flatten(Value& val, Environment& env)
{
    std::vector<Value> flattened;
    if (!val.is_list())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_list();
        for (size_t i = 0; i < valList.size(); i++)
        {
            Value evaluatedElement = valList[i].eval(env);
            if (evaluatedElement.is_list())
            {
                auto flattenedElement = flatten(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(), flattenedElement.end());
            }
            else
            {
                flattened.push_back(evaluatedElement);
            }
        }
    }
    return Value(flattened);
}

BUILTINFUNC_MEMBER(useq_fromFlattenedList, auto lst = flatten(args[0], env).as_list();
                   double phasor = args[1].as_float(); ret = fromList(lst, phasor, env);, 2)
BUILTINFUNC_MEMBER(useq_flatten, ret = flatten(args[0], env);, 1)
BUILTINFUNC_MEMBER(
    useq_interpolate, auto lst = args[0].as_list(); double phasor = args[1].as_float();
    if (phasor < 0.0) { phasor = 0; } else if (phasor > 1) { phasor = 1; } float a;
    double index = phasor * (lst.size() - 1); size_t pos0 = static_cast<size_t>(index);
    if (pos0 == (lst.size() - 1)) pos0--; a = (index - pos0); double v2 = lst[pos0 + 1].eval(env).as_float();
    double v1 = lst[pos0].eval(env).as_float(); ret = Value(((v2 - v1) * a) + v1);, 2)

// (step <phasor> <count> (<offset>))
BUILTINFUNC_VARGS_MEMBER(useq_step, const double phasor = args[0].as_float();
                         const int count     = args[1].as_int();
                         const double offset = (args.size() == 3) ? args[2].as_float() : 0;
                         double val          = static_cast<int>(phasor * abs(count)); if (val == count) val--;
                         ret                 = Value((count > 0 ? val : count - 1 - val) + offset);, 2, 3)

void uSEQ::setTimeSignature(double numerator, double denominator)
{
    meter_denominator = denominator;
    meter_numerator   = numerator;
    setBpm(bpm);
}

// NOTE lambdas are required because the map expects callable functions
// and so we have to capture the current object
// #define BUILTIN_MAP_INSERT_LAMBDA(__map__, __name__, __func_name__) \
//     __map__[__name__] = std::move(Value("__name__", [this](std::vector<Value>& args, Environment& env) \
//                                         { return this->__func_name__(args, env); }))
#define INSERT_BUILTIN_LAMBDA(__env__, __name__, __func_name__)                                              \
    __env__.set(__name__, Value(__name__, BuiltinFunc([this](std::vector<Value>& args, Environment& env)     \
                                                      { return this->__func_name__(args, env); })))

void uSEQ::insert_own_builtinfuncs()
{
    INSERT_BUILTIN_LAMBDA(m_interpreter, "fast", lisp_fast);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "slow", lisp_slow);

    INSERT_BUILTIN_LAMBDA(m_interpreter, "setbpm", useq_setbpm);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "getbpm", useq_getbpm);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "settimesig", useq_settimesig);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "schedule", lisp_schedule);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "unschedule", lisp_unschedule);

    INSERT_BUILTIN_LAMBDA(m_interpreter, "looph", useq_loopPhasor);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "dm", useq_dm);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "gates", useq_gates);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "gatesw", useq_gatesw);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "trigs", useq_trigs);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "euclid", useq_euclidean);
    // NOTE: different names for the same function
    INSERT_BUILTIN_LAMBDA(m_interpreter, "fromList", useq_fromList);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "seq", useq_fromList);
    //
    INSERT_BUILTIN_LAMBDA(m_interpreter, "flatIdx", useq_fromFlattenedList);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "flat", useq_flatten);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "interp", useq_interpolate);
    INSERT_BUILTIN_LAMBDA(m_interpreter, "step", useq_step);
}
