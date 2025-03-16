#include "uSEQ.h"
#include "uSEQ/piopwm.h"



// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level)
{
    DBG("uSEQ::pio_pwm_set_level");
    dbg(String(reinterpret_cast<size_t>(pio)));
    dbg(String(sm));
    dbg(String(level));
    pio_sm_put_blocking(pio, sm, level);
}

void pio_pwm_set_period(PIO pio, uint sm, uint32_t period)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

bool timer_callback(repeating_timer_t* mst)
{
    static float pdm_y   = 0;
    static float pdm_err = 0;
    static float pdm_w   = 0;
    pdm_y   = pdm_w > pdm_err ? 1 : 0;
    pdm_err = pdm_y - pdm_w + pdm_err;
    if (pdm_y == 1)
    {
        // on
        digitalWrite(USEQ_PIN_LED_AI1, HIGH);
    }
    else
    {
        // off
        digitalWrite(USEQ_PIN_LED_AI1, LOW);
    }

    //   w = w + 0.00000001;
    //   if (w>=1) w=0;

    return true;
}

void start_pdm()
{
    static repeating_timer_t mst;

    add_repeating_timer_us(150, timer_callback, NULL, &mst);
}

int analog_out_LED_pin(int out)
{
    int res = -1;
    if (out <= NUM_CONTINUOUS_OUTS)
        res = useq_output_led_pins[out - 1];
    return res;
}

int digital_out_LED_pin(int out)
{
    int res    = -1;
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex <= 6)
        res = useq_output_led_pins[pindex - 1];
    return res;
}

int analog_out_pin(int out)
{
    int res = -1;
    if (out <= NUM_CONTINUOUS_OUTS)
        res = useq_output_pins[out - 1];
    return res;
}

int digital_out_pin(int out)
{
    int res    = -1;
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex <= 6)
        res = useq_output_pins[pindex - 1];
    return res;
}

/*
    const auto input1    = 1 - digitalRead(USEQ_PIN_I1);
    const auto input2    = 1 - digitalRead(USEQ_PIN_I2);
    m_input_vals[USEQI1] = input1;
    m_input_vals[USEQI2] = input2;

    digitalWrite(USEQ_PIN_LED_I1, input1);
    digitalWrite(USEQ_PIN_LED_I2, input2);

    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);
*/

void uSEQ::set_input_val(size_t index, double value) { m_input_vals[index] = value; }

void uSEQ::gpio_irq_gate1()
{
    double ts         = static_cast<double>(micros());
    const auto input1 = 1 - digitalRead(USEQ_PIN_I1);
    uSEQ::instance->set_input_val(USEQI1, input1);
    digitalWrite(USEQ_PIN_LED_I1, input1);
    if (input1 == 1 &&
        uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I1)
    {
        uSEQ::instance->update_clock_from_external(ts);
    }
}

void uSEQ::gpio_irq_gate2()
{
    double ts         = static_cast<double>(micros());
    const auto input2 = 1 - digitalRead(USEQ_PIN_I2);
    uSEQ::instance->set_input_val(USEQI2, input2);
    digitalWrite(USEQ_PIN_LED_I2, input2);
    if (input2 == 1 &&
        uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I2)
    {
        uSEQ::instance->update_clock_from_external(ts);
    }
}

void uSEQ::analog_write_with_led(int output, double val)
{
    DBG("uSEQ::analog_write_with_led");

    constexpr double maxpwm = 2047.0;

    int scaled_val = val * maxpwm;
    dbg("scaled_val (before clamping) = " + String(scaled_val));

    // clamping
    if (scaled_val > maxpwm)
    {
        dbg("over maxpwm, clamping");
        scaled_val = maxpwm;
    }
    if (scaled_val < 0)
    {
        dbg("less than 0, clamping");
        scaled_val = 0;
    }

    // led
    // int led_pin   = analog_out_LED_pin(output + 1);
    int pwm_pin   = analog_out_pin(output + 1);
    int ledsigval = scaled_val; // >> 2; // shift to 11 bit range for the LED

    ledsigval =
        (ledsigval * ledsigval) >> 11; // cheap way to square and get a exp curve

    dbg("output = " + String(output));
    dbg("pin = " + String(pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));
    dbg("scaled_val = " + String(scaled_val));

    // // write pwm
    // pio_pwm_set_level(output < 4 ? pio0 : pio1, output % 4, scaled_val);
    // // write led
    // analogWrite(led_pin, ledsigval);

    // write pwm
    pio_pwm_set_level(pio0, output, ledsigval);
    // write led
    analogWrite(pwm_pin, scaled_val);
}

// NOTE: outputs are 0-indexed,
void uSEQ::digital_write_with_led(int output, int val)
{
    DBG("uSEQ::digital_write_with_led");

    int pin     = digital_out_pin(output + 1);
    int led_pin = digital_out_LED_pin(output + 1);

    dbg("output = " + String(output));
    dbg("pin = " + String(pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));

    // write digi
#ifdef DIGI_OUT_INVERT
    digitalWrite(pin, 1 - (val > 0));
#else
    digitalWrite(pin, val > 0);
#endif
    // write led
    digitalWrite(led_pin, val > 0);
}

void uSEQ::update_clock_from_external(double ts)
{
    double newBPM = tempoI1.averageBPM(ts);
    if (ext_clock_tracker.count == 0)
    {
        newBPM *= (4.0 / meter_numerator / ext_clock_tracker.div);
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
        if (meter_denominator == ext_clock_tracker.beat_count)
        {
            ext_clock_tracker.beat_count = 0;
            ext_clock_tracker.bar_count++;
            if (ext_clock_tracker.bar_count ==
                static_cast<size_t>(m_bars_per_phrase))
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