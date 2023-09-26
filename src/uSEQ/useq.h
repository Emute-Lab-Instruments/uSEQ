#ifndef USEQ_H_
#define USEQ_H_

/*
 ------------------------------------------------------------------------------
| Copyright Dimitris Kyriakoudis and Chris Kiefer 2022. | | | | This source
describes Open Hardware and is licensed under the CERN-OHL-S v2. | | | | You may
redistribute and modify this source and make products using it under | | the
terms of the CERN-OHL-S v2 (https://ohwr.org/cern_ohl_s_v2.txt).         | | |
| This source is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY,          |
| INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A         |
| PARTICULAR PURPOSE. Please see the CERN-OHL-S v2 for applicable conditions.  |
|                                                                              |
| Source location: https://github.com/lnfiniteMonkeys/uSEQ | | | | As per
CERN-OHL-S v2 section 4, should You produce hardware based on this    | |
source, You must where practicable maintain the Source Location visible      |
| on the external case of the Gizmo or other products you make using this      |
| source.                                                                      |
 ------------------------------------------------------------------------------

 */
// uSEQ firmware, by Dimitris Kyriakoudis and Chris Kiefer

/// LISP interpreter forked from Wisp, by Adam McDaniel

// firmware build options (comment out as needed)

// Not sure where the best place to put this is, needs to be accessible
// by all interpreter functions that may need to raise it
bool currentExprSound = false;

#define USEQ_NUM_DIGITAL_OUTPUTS 4
#define USEQ_NUM_DIGITAL_INPUTS 0

#define USEQ_NUM_ANALOG_OUTPUTS 2
#define USEQ_NUM_ANALOG_INPUTS 2

#define MIDIOUT // (drum sequencer implemented using (mdo note (f t)))
// #define MIDIIN //(to be implemented)

// end of build options

#include "LispLibrary.h"
#include "MAFilter.hpp"
#include "pinmap.h"

#define ETL_NO_STL
#include <Embedded_Template_Library.h> // Mandatory for Arduino IDE only
#include <etl/map.h>
#include <etl/string.h>
#include <etl/unordered_map.h>
String board(ARDUINO_BOARD);

////////////////////////////////////////////////////////////////////////////////
/// LISP LIBRARY /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

int ts1 = 0;
int ts_total = 0;
int timer_level = 0;
#define RESET_TIMER                                                            \
  ts_total = 0;                                                                \
  timer_level = 0;
#define START_TIMER                                                            \
  if (timer_level == 0) {                                                      \
    ts1 = micros();                                                            \
  };                                                                           \
  timer_level++;
#define STOP_TIMER                                                             \
  timer_level--;                                                               \
  if (timer_level == 0)                                                        \
    ts_total += (micros() - ts1);

enum useqInputNames {
  // signals
  USEQI1 = 0,
  USEQI2 = 1,
  // momentary
  USEQM1 = 2,
  USEQM2 = 3,
  // toggle
  USEQT1 = 4,
  USEQT2 = 5,
  // rotary enoder
  USEQRS1 = 6, // switch
  USEQR1 = 7   // encoder
};

int useqInputValues[8];

#define NO_LIBM_SUPPORT "no libm support"

// Comment this define out to drop support for standard library functions.
// This allows the program to run without a runtime.
#define USE_STD
#ifdef USE_STD
#else
#define NO_STD "no standard library support"
#endif

int digital_out_pin(int out) {
  switch (out) {
  case 99:
    return LED_BOARD;
  case 1:
    return USEQ_PIN_D1;
  case 2:
    return USEQ_PIN_D2;
  case 3:
    return USEQ_PIN_D3;
  case 4:
    return USEQ_PIN_D4;
  default:
    return -1;
  }
}

int digital_out_LED_pin(int out) {
  switch (out) {
  // Builtin LED
  case 1:
    return USEQ_PIN_LED_D1;
  case 2:
    return USEQ_PIN_LED_D2;
  case 3:
    return USEQ_PIN_LED_D3;
  case 4:
    return USEQ_PIN_LED_D4;
  default:
    return -1;
  }
}

int analog_out_pin(int out) {
  switch (out) {
  // Analog Pins
  case 1:
    return USEQ_PIN_A1;
  case 2:
    return USEQ_PIN_A2;
  default:
    return -1;
  }
}

int analog_out_LED_pin(int out) {
  switch (out) {
  // Analog Pins
  case 1:
    return USEQ_PIN_LED_A1;
  case 2:
    return USEQ_PIN_LED_A2;
  default:
    return -1;
  }
}

///////////////////////////
// USEQ NAMESPACE
///////////////////////////

Environment env;

namespace useq {

// Meter
double meter_numerator = 4;
double meter_denominator = 4;

// TODO: better to have custom specified long phasors e.g. (addPhasor phasorName
// (lambda () (beats * 17))) - to be stored in std::map<String, Double[2]>
double barsPerPhrase = 16;
double phrasesPerSection = 16;

// BPM
double defaultBpm = 130.0;
double bpm = 130.0;
double bps = 0.0;

// TODO: builtin functions to set phasor lengths and other timing
// phasor lengths
double beatDur = 0.0;
double barDur = 0.0;
double phraseDur = 0.0;
double sectionDur = 0.0;

// Timing
double lastResetTime = millis();
double time = 0;
double t = 0;      // time since last reset
double last_t = 0; // timestamp of the previous time update (since reset)
double beat = 0.0;
double bar = 0.0;
double phrase = 0.0;
double section = 0.0;

std::vector<Value> digitalASTs[USEQ_NUM_DIGITAL_OUTPUTS];
std::vector<Value> analogASTs[USEQ_NUM_ANALOG_OUTPUTS];

void updateBpmVariables() {
  env.set("bpm", Value(bpm));
  env.set("bps", Value(bps));
  env.set("beatDur", Value(beatDur));
  env.set("barDur", Value(barDur));
  env.set("phraseDur", Value(phraseDur));
  env.set("sectionDur", Value(sectionDur));
}

void setBpm(double newBpm) {
  bpm = newBpm;
  bps = bpm / 60.0;

  beatDur = 1000.0 / bps;
  barDur = beatDur * (4.0 / meter_denominator) * meter_numerator;
  phraseDur = barDur * barsPerPhrase;
  sectionDur = phraseDur * phrasesPerSection;

  updateBpmVariables();
}

void setTimeSignature(double numerator, double denominator) {
  meter_denominator = denominator;
  meter_numerator = numerator;
  setBpm(bpm);
}

void updateTimeVariables() {
  env.set("time", Value(time));
  env.set("t", Value(t));

  // phasors
  env.set("beat", Value(beat));
  env.set("bar", Value(bar));
  env.set("phrase", Value(phrase));
  env.set("section", Value(section));
}

// Set the module's "transport" to a specified value in microseconds
// and update all derrivative variables
void setTime(size_t newTimeMillis) {
  time = newTimeMillis;
  // last_t = t;
  t = newTimeMillis - lastResetTime;
  beat = fmod(t, beatDur) / beatDur;
  bar = fmod(t, barDur) / barDur;
  phrase = fmod(t, phraseDur) / phraseDur;
  section = fmod(t, sectionDur) / sectionDur;

  updateTimeVariables();
}

// Update time to the current value of `micros()`
// and update each variable that's derrived from it
void updateTime() { setTime(millis()); }

void resetTime() {
  lastResetTime = millis();
  updateTime();
}

void updateDigitalOutputs();
void updateAnalogOutputs();

#ifdef MIDIOUT
double last_midi_t = 0;
void updateMidiOut() {
  const double midiRes = 48 * meter_numerator * 1;
  const double timeUnitMillis = (barDur / midiRes);

  const double timeDeltaMillis = t - last_midi_t;
  size_t steps = floor(timeDeltaMillis / timeUnitMillis);
  double initValPhase = bar - (timeDeltaMillis / barDur);

  if (steps > 0) {
    const double timeUnitBar = 1.0 / midiRes;

    auto itr = useqMDOMap.begin();
    for (; itr != useqMDOMap.end(); itr++) {
      // Iterate through the keys process MIDI events
      Value midiFunction = itr->second;
      if (initValPhase < 0)
        initValPhase++;
      std::vector<Value> mdoArgs = {Value(initValPhase)};
      Value prev = midiFunction.apply(mdoArgs, env);
      for (size_t step = 0; step < steps; step++) {
        double t_step = bar - ((steps - (step + 1)) * timeUnitBar);
        // wrap phasor
        if (t_step < 0)
          t_step += 1.0;
        // Serial.println(t_step);
        mdoArgs[0] = Value(t_step);
        Value val = midiFunction.apply(mdoArgs, env);

        // Serial.println(val.as_float());
        if (val > prev) {
          Serial1.write(0x99);
          Serial1.write(itr->first);
          Serial1.write(val.as_int() * 14);
        } else if (val < prev) {
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
#endif

MovingAverageFilter cqpMA(3); // code quantising phasor
double lastCQP = 0;
String cqpCode = "(define cqp 'bar)";
std::vector<Value> cqpAST;

std::vector<std::vector<Value>> runQueue;

void initASTs() {
  for (int i = 0; i < USEQ_NUM_DIGITAL_OUTPUTS; i++)
    digitalASTs[i] = {parse("(sqr beat)")[0]};

  for (int i = 0; i < USEQ_NUM_ANALOG_OUTPUTS; i++)
    analogASTs[i] = {parse("(sqr bar)")[0]};
}

void setup() {
  setBpm(defaultBpm);
  updateTime();
  // env.set_global("cqp", ::parse(cqpCode));
  // run(cqpCode, env);
  cqpAST = ::parse("(eval 'bar)");
  initASTs();
}

void updateDigitalOutputs() {
  for (size_t i = 0; i < USEQ_NUM_DIGITAL_OUTPUTS; i++) {
    // We assume that the expression is correct unless proven otherwise
    currentExprSound = true;

    Value result;
    result = runParsedCode(digitalASTs[i], env);

    // Check for errors
    if (result == Value::error() || !currentExprSound) {
      Serial.println("Error in form, clearing...");
      digitalASTs[i] = {Value((int)0)};
    }
    // Write
    else {
      int pin = digital_out_pin(i + 1);
      int led_pin = digital_out_LED_pin(i + 1);
      int val = result.as_int();
      digitalWrite(pin, val);
      digitalWrite(led_pin, val);
    }
  }
}

void updateAnalogOutputs() {
  for (size_t i = 0; i < USEQ_NUM_ANALOG_OUTPUTS; i++) {
    // We assume that the expression is correct unless proven otherwise
    currentExprSound = true;

    Value result;
    result = runParsedCode(analogASTs[i], env);

    // Check for errors
    if (result == Value::error() || !currentExprSound) {
      Serial.println("Error in form, clearing...");
      analogASTs[i] = {Value((int)0)};
    }
    // Write
    else {
      int pin = analog_out_pin(i + 1);
      int led_pin = analog_out_LED_pin(i + 1);
      int val = result.as_float() * 2047.0;
      analogWrite(pin, val);
      analogWrite(led_pin, val);
    }
  }
}

int ts_inputs = 0, ts_time = 0, ts_outputs = 0;

void update() {
  ts_inputs = millis();
  readInputs();
  env.set("perf_in", Value(int(millis() - ts_inputs)));
  ts_time = millis();
  updateTime();
  env.set("perf_time", Value(int(millis() - ts_time)));
  ts_outputs = millis();

  // check code quant phasor
  //  double newCqpVal = run("(eval bar)", env).as_float();
  double newCqpVal = runParsedCode(cqpAST, env).as_float();
  double cqpAvgTime = cqpMA.process(newCqpVal - lastCQP);
  // Serial.println(newCqpVal);
  lastCQP = newCqpVal;
  if (newCqpVal + cqpAvgTime > 1) {

    for (size_t q = 0; q < runQueue.size(); q++) {
      Value res;
      int cmdts = micros();
      res = runParsedCode(runQueue[q], env);
      cmdts = micros() - cmdts;
      Serial.println(res.debug());
    }
    runQueue.clear();
  }

  /* run("(eval q-form)", env);   */
  updateAnalogOutputs();
  updateDigitalOutputs();
#ifdef MIDIOUT
  updateMidiOut();
#endif

  env.set("perf_out", Value(int(millis() - ts_outputs)));
}

} // namespace useq

// extra arduino api functions
namespace builtin {

double fast(double speed, double phasor) {
  phasor *= speed;
  double phase = fmod(phasor, 1.0);
  return phase;
}

Value fromList(std::vector<Value> &lst, double phasor, Environment &env) {
  if (phasor < 0.0) {
    phasor = 0;
  } else if (phasor > 1) {
    phasor = 1;
  }
  double scaled_phasor = lst.size() * phasor;
  size_t idx = floor(scaled_phasor);
  if (idx == lst.size())
    idx--;
  return lst[idx].eval(env);
}

/* BUILTINFUNC_NOEVAL(useq_q0, env.set_global("q-form", args[0]);, 1) */

BUILTINFUNC_NOEVAL(a1, env.set_global("a1-form", args[0]);
                   useq::analogASTs[0] = {args[0]};
                   // run("(useqaw 1 (eval a1-form))", env);
                   , 1)
BUILTINFUNC_NOEVAL(a2, env.set_global("a2-form", args[0]);
                   useq::analogASTs[1] = {args[0]};, 1)

BUILTINFUNC_NOEVAL(d1, env.set_global("d1-form", args[0]);
                   useq::digitalASTs[0] = {args[0]};, 1)
BUILTINFUNC_NOEVAL(d2, env.set_global("d2-form", args[0]);
                   useq::digitalASTs[1] = {args[0]};, 1)
BUILTINFUNC_NOEVAL(d3, env.set_global("d3-form", args[0]);
                   useq::digitalASTs[2] = {args[0]};, 1)
BUILTINFUNC_NOEVAL(d4, env.set_global("d4-form", args[0]);
                   useq::digitalASTs[3] = {args[0]};, 1)

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC(
    useq_mdo, int midiNote = args[0].as_int(); if (args[1] != 0) {
      useqMDOMap[midiNote] = args[1];
    } else { useqMDOMap.erase(midiNote); },
                                               2)
#endif

BUILTINFUNC(ard_pinMode, int pinNumber = args[0].as_int();
            int onOff = args[1].as_int(); pinMode(pinNumber, onOff);, 2)

BUILTINFUNC(
    ard_useqdw,
    if (args[1] == Value::error()) {
      Serial.println("useqdw arg err");
      ret = args[1];
    } else {
      int pin = digital_out_pin(args[0].as_int());
      int led_pin = digital_out_LED_pin(args[0].as_int());
      int val = args[1].as_int();
      digitalWrite(pin, val);
      digitalWrite(led_pin, val);
    },
    2)

BUILTINFUNC(
    ard_useqaw,
    if (args[1] == Value::error()) {
      Serial.println("useqaw arg err");
      ret = args[1];
    } else {
      int pin = analog_out_pin(args[0].as_int());
      int led_pin = analog_out_LED_pin(args[0].as_int());
      int val = args[1].as_float() * 2047.0;
      analogWrite(pin, val);
      analogWrite(led_pin, val);
    },
    2)

BUILTINFUNC(ard_digitalWrite, int pinNumber = args[0].as_int();
            int onOff = args[1].as_int(); digitalWrite(pinNumber, onOff);
            ret = args[0];, 2)

BUILTINFUNC(ard_digitalRead, int pinNumber = args[0].as_int();
            int val = digitalRead(pinNumber); ret = Value(val);, 1)

BUILTINFUNC(ard_delay, int delaytime = args[0].as_int(); delay(delaytime);
            ret = args[0];, 1)

BUILTINFUNC(ard_delaymicros, int delaytime = args[0].as_int();
            delayMicroseconds(delaytime); ret = args[0];, 1)

BUILTINFUNC(ard_millis, int m = millis(); ret = Value(m);, 0)

BUILTINFUNC(ard_micros, int m = micros(); ret = Value(m);, 0)

BUILTINFUNC(useq_pulse,
            // args: pulse width, phasor
            double pulseWidth = args[0].as_float();
            double phasor = args[1].as_float();
            ret = Value(pulseWidth < phasor ? 1.0 : 0.0);, 2)
BUILTINFUNC(useq_sqr, ret = Value(args[0].as_float() < 0.5 ? 1.0 : 0.0);, 1)
BUILTINFUNC(useq_fast, double speed = args[0].as_float();
            double phasor = args[1].as_float();
            double fastPhasor = fast(speed, phasor); ret = Value(fastPhasor);
            , 2)
BUILTINFUNC(useq_fromList, auto lst = args[0].as_list();
            double phasor = args[1].as_float();
            ret = fromList(lst, phasor, env);, 2)
BUILTINFUNC(useq_fromFlattenedList, auto lst = flatten(args[0], env).as_list();
            double phasor = args[1].as_float();
            ret = fromList(lst, phasor, env);, 2)
BUILTINFUNC(useq_flatten, ret = flatten(args[0], env);, 1)
BUILTINFUNC(
    useq_interpolate, auto lst = args[0].as_list();
    double phasor = args[1].as_float();
    if (phasor < 0.0) { phasor = 0; } else if (phasor > 1) {
      phasor = 1;
    } double scaled_phasor = lst.size() * phasor;
    size_t idx = static_cast<size_t>(scaled_phasor) + 1;
    if (idx == lst.size()) idx--; double v2 = lst[idx].eval(env).as_float();
    size_t idxv1 = idx == 0 ? lst.size() - 1 : idx - 1;
    double v1 = lst[idxv1].eval(env).as_float();
    double relativePosition = scaled_phasor - idx;
    ret = Value((v1 * relativePosition) + (v2 * (1.0 - relativePosition)));
    // ret = fromList(lst, phasor, env);
    , 2)

BUILTINFUNC(useq_dm, auto index = args[0].as_int();
            auto v1 = args[1].as_float(); auto v2 = args[2].as_float();
            ret = Value(index > 0 ? v2 : v1);, 3)

BUILTINFUNC_VARGS(
    useq_gates, auto lst = args[0].as_list();
    double phasor = args[1].as_float(); double speed = args[2].as_float();
    double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.5;
    double val = fromList(lst, fast(speed, phasor), env).as_int();
    double gates = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
    ret = Value(val * gates);, 3, 4)

BUILTINFUNC_VARGS(useq_gatesw, auto lst = args[0].as_list();
                  double phasor = args[1].as_float();
                  double speed = args.size() == 3 ? args[2].as_float() : 1.0;
                  double val = fromList(lst, fast(speed, phasor), env).as_int();
                  double pulseWidth = val / 9.0;
                  double gate = fast(speed * lst.size(), phasor) < pulseWidth
                                    ? 1.0
                                    : 0.0;
                  ret = Value((val > 0 ? 1.0 : 0.0) * gate);, 2, 3)

BUILTINFUNC(useq_loopPhasor,
            auto phasor = args[0].as_float();
            auto loopPoint = args[1].as_float();
            if (loopPoint == 0) loopPoint = 1; // avoid infinity
            double spedupPhasor = fast(1.0 / loopPoint, phasor);
            ret = spedupPhasor * loopPoint;, 2)

BUILTINFUNC(useq_setbpm, useq::setBpm(args[0].as_float()); ret = args[0];, 1)

BUILTINFUNC(useq_settimesig,
            useq::setTimeSignature(args[0].as_float(), args[1].as_float());
            ret = Value(1);, 2)

BUILTINFUNC(useq_in1, ret = Value(useqInputValues[USEQI1]);, 0)
BUILTINFUNC(useq_in2, ret = Value(useqInputValues[USEQI2]);, 0)

BUILTINFUNC(
    useq_swm, int index = args[0].as_int(); if (index == 1) {
      ret = Value(useqInputValues[USEQM1]);
    } else { ret = Value(useqInputValues[USEQM2]); },
                                            1)

BUILTINFUNC(
    useq_swt, int index = args[0].as_int(); if (index == 1) {
      ret = Value(useqInputValues[USEQT1]);
    } else { ret = Value(useqInputValues[USEQT2]); },
                                            1)

BUILTINFUNC(useq_swr, ret = Value(useqInputValues[USEQRS1]);, 0)

BUILTINFUNC(useq_rot, ret = Value(useqInputValues[USEQR1]);, 0)

BUILTINFUNC(perf,

            String report = "fps0: ";
            report += env.get("fps").as_float();
            // report += ", fps1: ";
            // report += env.get("perf_fps1").as_int();
            report += ", qt: ";
            report += env.get("qt").as_float(); report += ", in: ";
            report += env.get("perf_in").as_int(); report += ", upd_tm: ";
            report += env.get("perf_time").as_int(); report += ", out: ";
            report += env.get("perf_out").as_int(); report += ", get: ";
            report += env.get("perf_get").as_float(); report += ", parse: ";
            report += env.get("perf_parse").as_float(); report += ", run: ";
            report += env.get("perf_run").as_float(); report += ", ts1: ";
            report += env.get("perf_ts1").as_float(); report += ", heap free: ";
            report += rp2040.getFreeHeap() / 1024; Serial.println(report);
            ret = Value();, 0)

} // namespace builtin

void loadBuiltinDefs() {
  Environment::builtindefs["useqdw"] = Value("useqdw", builtin::ard_useqdw);
  Environment::builtindefs["useqaw"] = Value("useqaw", builtin::ard_useqaw);
  Environment::builtindefs["a1"] = Value("a1", builtin::a1);
  Environment::builtindefs["a2"] = Value("a2", builtin::a2);
  Environment::builtindefs["d1"] = Value("d1", builtin::d1);
  Environment::builtindefs["d2"] = Value("d2", builtin::d2);
  Environment::builtindefs["d3"] = Value("d3", builtin::d3);
  Environment::builtindefs["d4"] = Value("d4", builtin::d4);
  /* Environment::builtindefs["q0"] = Value("q0", builtin::useq_q0); */

  Environment::builtindefs["pm"] = Value("pm", builtin::ard_pinMode);
  Environment::builtindefs["dw"] = Value("dw", builtin::ard_digitalWrite);
  Environment::builtindefs["dr"] = Value("dr", builtin::ard_digitalRead);
  Environment::builtindefs["delay"] = Value("delay", builtin::ard_delay);
  Environment::builtindefs["delaym"] =
      Value("delaym", builtin::ard_delaymicros);
  Environment::builtindefs["millis"] = Value("millis", builtin::ard_millis);
  Environment::builtindefs["micros"] = Value("micros", builtin::ard_micros);
  Environment::builtindefs["perf"] = Value("perf", builtin::perf);
  Environment::builtindefs["in1"] = Value("in1", builtin::useq_in1);
  Environment::builtindefs["in2"] = Value("in2", builtin::useq_in2);
  Environment::builtindefs["swm"] = Value("swm", builtin::useq_swm);
  Environment::builtindefs["swt"] = Value("swt", builtin::useq_swt);
  Environment::builtindefs["swr"] = Value("swr", builtin::useq_swr);
  Environment::builtindefs["rot"] = Value("rot", builtin::useq_rot);

  // sequencing
  Environment::builtindefs["pulse"] = Value("pulse", builtin::useq_pulse);
  Environment::builtindefs["sqr"] = Value("sqr", builtin::useq_sqr);
  Environment::builtindefs["fast"] = Value("fast", builtin::useq_fast);
  Environment::builtindefs["fromList"] =
      Value("fromList", builtin::useq_fromList);
  Environment::builtindefs["flatIdx"] =
      Value("flatIdx", builtin::useq_fromFlattenedList);
  Environment::builtindefs["flat"] = Value("flat", builtin::useq_flatten);
  Environment::builtindefs["looph"] = Value("looph", builtin::useq_loopPhasor);

  Environment::builtindefs["dm"] = Value("dm", builtin::useq_dm);
  Environment::builtindefs["gates"] = Value("gates", builtin::useq_gates);
  Environment::builtindefs["gatesw"] = Value("gatesw", builtin::useq_gatesw);
  Environment::builtindefs["setbpm"] = Value("setbpm", builtin::useq_setbpm);
  Environment::builtindefs["settimesig"] =
      Value("settimesig", builtin::useq_settimesig);

  Environment::builtindefs["interp"] =
      Value("interp", builtin::useq_interpolate);

#ifdef MIDIOUT
  Environment::builtindefs["mdo"] = Value("mdo", builtin::useq_mdo);
#endif
  // arduino math
  Environment::builtindefs["sin"] = Value("sin", builtin::ard_sin);
  Environment::builtindefs["cos"] = Value("cos", builtin::ard_cos);
  Environment::builtindefs["tan"] = Value("tan", builtin::ard_tan);
  Environment::builtindefs["abs"] = Value("abs", builtin::ard_abs);

  Environment::builtindefs["min"] = Value("min", builtin::ard_min);
  Environment::builtindefs["max"] = Value("max", builtin::ard_max);
  Environment::builtindefs["pow"] = Value("pow", builtin::ard_pow);
  Environment::builtindefs["sqrt"] = Value("sqrt", builtin::ard_sqrt);
  Environment::builtindefs["scale"] = Value("scale", builtin::ard_map);

  // Meta operations
  Environment::builtindefs["eval"] = Value("eval", builtin::eval);
  Environment::builtindefs["type"] = Value("type", builtin::get_type_name);
  Environment::builtindefs["parse"] = Value("parse", builtin::parse);

  // Special forms
  Environment::builtindefs["do"] = Value("do", builtin::do_block);
  Environment::builtindefs["if"] = Value("if", builtin::if_then_else);
  Environment::builtindefs["for"] = Value("for", builtin::for_loop);
  Environment::builtindefs["while"] = Value("while", builtin::while_loop);
  Environment::builtindefs["scope"] = Value("scope", builtin::scope);
  Environment::builtindefs["quote"] = Value("quote", builtin::quote);
  Environment::builtindefs["defun"] = Value("defun", builtin::defun);
  Environment::builtindefs["define"] = Value("define", builtin::define);
  Environment::builtindefs["set"] = Value("set", builtin::set);
  Environment::builtindefs["lambda"] = Value("lambda", builtin::lambda);

  // Comparison operations
  Environment::builtindefs["="] = Value("=", builtin::eq);
  Environment::builtindefs["!="] = Value("!=", builtin::neq);
  Environment::builtindefs[">"] = Value(">", builtin::greater);
  Environment::builtindefs["<"] = Value("<", builtin::less);
  Environment::builtindefs[">="] = Value(">=", builtin::greater_eq);
  Environment::builtindefs["<="] = Value("<=", builtin::less_eq);

  // Arithmetic operations
  Environment::builtindefs["+"] = Value("+", builtin::sum);
  Environment::builtindefs["-"] = Value("-", builtin::subtract);
  Environment::builtindefs["*"] = Value("*", builtin::product);
  Environment::builtindefs["/"] = Value("/", builtin::divide);
  Environment::builtindefs["%"] = Value("%", builtin::remainder);
  Environment::builtindefs["floor"] = Value("floor", builtin::ard_floor);
  Environment::builtindefs["ceil"] = Value("ceil", builtin::ard_ceil);

  // List operations
  Environment::builtindefs["list"] = Value("list", builtin::list);
  Environment::builtindefs["insert"] = Value("insert", builtin::insert);
  Environment::builtindefs["index"] = Value("index", builtin::index);
  Environment::builtindefs["remove"] = Value("remove", builtin::remove);

  Environment::builtindefs["len"] = Value("len", builtin::len);

  Environment::builtindefs["push"] = Value("push", builtin::push);
  Environment::builtindefs["pop"] = Value("pop", builtin::pop);
  Environment::builtindefs["head"] = Value("head", builtin::head);
  Environment::builtindefs["tail"] = Value("tail", builtin::tail);
  Environment::builtindefs["first"] = Value("first", builtin::head);
  Environment::builtindefs["last"] = Value("last", builtin::pop);
  Environment::builtindefs["range"] = Value("range", builtin::range);

  // Functional operations
  Environment::builtindefs["map"] = Value("map", builtin::map_list);
  Environment::builtindefs["filter"] = Value("filter", builtin::filter_list);
  Environment::builtindefs["reduce"] = Value("reduce", builtin::reduce_list);

// IO operations
#ifdef USE_STD
  // if (name == "exit") return Value("exit", builtin::exit);
  // if (name == "quit") return Value("quit", builtin::exit);
  Environment::builtindefs["print"] = Value("print", builtin::print);
  // if (name == "input") return Value("input", builtin::input);
  Environment::builtindefs["random"] = Value("random", builtin::gen_random);
#endif

  // String operations
  Environment::builtindefs["debug"] = Value("debug", builtin::debug);
  Environment::builtindefs["replace"] = Value("replace", builtin::replace);
  Environment::builtindefs["display"] = Value("display", builtin::display);

  // Casting operations
  Environment::builtindefs["int"] = Value("int", builtin::cast_to_int);
  Environment::builtindefs["float"] = Value("float", builtin::cast_to_float);

  // Constants
  Environment::builtindefs["endl"] = Value::string("\n");
}

void flash_builtin_led(int num, int amt) {
  for (int i = 0; i < num; i++) {
    digitalWrite(LED_BUILTIN, 1);
    delay(amt);
    digitalWrite(LED_BUILTIN, 0);
    delay(amt);
  }
}

void setup_digital_outs() {
  pinMode(USEQ_PIN_D1, OUTPUT);
  pinMode(USEQ_PIN_D2, OUTPUT);
  pinMode(USEQ_PIN_D3, OUTPUT);
  pinMode(USEQ_PIN_D4, OUTPUT);
}

void setup_analog_outs() {
  pinMode(USEQ_PIN_A1, OUTPUT);
  pinMode(USEQ_PIN_A2, OUTPUT);

  // PWM outputs
  analogWriteFreq(30000);    // out of hearing range
  analogWriteResolution(11); // about the best we can get for 30kHz

  analogWrite(USEQ_PIN_A1, 0);
  analogWrite(USEQ_PIN_A2, 0);
}

void setup_digital_ins() {
  pinMode(USEQ_PIN_I1, INPUT_PULLUP);
  pinMode(USEQ_PIN_I2, INPUT_PULLUP);
}

void setup_leds() {
  pinMode(LED_BOARD, OUTPUT); // test LED

  pinMode(USEQ_PIN_LED_I1, OUTPUT);
  pinMode(USEQ_PIN_LED_I2, OUTPUT);
  pinMode(USEQ_PIN_LED_A1, OUTPUT);
  pinMode(USEQ_PIN_LED_A2, OUTPUT);

  pinMode(USEQ_PIN_LED_D1, OUTPUT);
  pinMode(USEQ_PIN_LED_D2, OUTPUT);
  pinMode(USEQ_PIN_LED_D3, OUTPUT);
  pinMode(USEQ_PIN_LED_D4, OUTPUT);

  digitalWrite(LED_BOARD, 1);
}

void setup_switches() {
  pinMode(USEQ_PIN_SWITCH_M1, INPUT_PULLUP);
  pinMode(USEQ_PIN_SWITCH_M2, INPUT_PULLUP);

  pinMode(USEQ_PIN_SWITCH_T1, INPUT_PULLUP);
  pinMode(USEQ_PIN_SWITCH_T2, INPUT_PULLUP);
}

void setup_rotary_encoder() {
  pinMode(USEQ_PIN_SWITCH_R1, INPUT_PULLUP);
  pinMode(USEQ_PIN_ROTARYENC_A, INPUT_PULLUP);
  pinMode(USEQ_PIN_ROTARYENC_B, INPUT_PULLUP);
  useqInputValues[USEQR1] = 0;
}

void led_animation() {
  int ledDelay = 30;
  for (int i = 0; i < 8; i++) {
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
}

void setup_IO() {
  setup_digital_outs();
  setup_analog_outs();
  setup_digital_ins();
  setup_switches();
  setup_rotary_encoder();

#ifdef MIDIOUT
  Serial1.setRX(1);
  Serial1.setTX(0);
  Serial1.begin(31250);
#endif
}

void module_setup() { setup_IO(); }

static uint8_t prevNextCode = 0;
static uint16_t store = 0;

int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0, 1, 1, 0, 1, 0, 0, 1,
                                   1, 0, 0, 1, 0, 1, 1, 0};

  prevNextCode <<= 2;
  if (digitalRead(USEQ_PIN_ROTARYENC_B))
    prevNextCode |= 0x02;
  if (digitalRead(USEQ_PIN_ROTARYENC_A))
    prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

  // If valid then store as 16 bit data.
  if (rot_enc_table[prevNextCode]) {
    store <<= 4;
    store |= prevNextCode;
    // if (store==0xd42b) return 1;
    // if (store==0xe817) return -1;
    if ((store & 0xff) == 0x2b)
      return -1;
    if ((store & 0xff) == 0x17)
      return 1;
  }
  return 0;
}

void readRotaryEnc() {
  static int32_t c, val;

  if (val = read_rotary()) {
    useqInputValues[USEQR1] += val;
    // Serial.print(c);Serial.print(" ");
  }
}

void readInputs() {
  // inputs are input_pullup, so invert
  useqInputValues[USEQI1] = 1 - digitalRead(USEQ_PIN_I1);
  useqInputValues[USEQI2] = 1 - digitalRead(USEQ_PIN_I2);
  digitalWrite(USEQ_PIN_LED_I1, useqInputValues[USEQI1]);
  digitalWrite(USEQ_PIN_LED_I2, useqInputValues[USEQI2]);
  useqInputValues[USEQRS1] = 1 - digitalRead(USEQ_PIN_SWITCH_R1);

  useqInputValues[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);
  useqInputValues[USEQM2] = 1 - digitalRead(USEQ_PIN_SWITCH_M2);
  useqInputValues[USEQT1] = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
  useqInputValues[USEQT2] = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
}

int test = 0;

bool setupComplete = false;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.setTimeout(2);
  randomSeed(analogRead(0));

  loadBuiltinDefs();

  setup_leds();
  led_animation();
  module_setup();

  for (int i = 0; i < LispLibrarySize; i++)
    run(LispLibrary[i], env);
  Serial.println("Library loaded");
  useq::setup();
  setupComplete = true;
  // multicore_launch_core1(loop_core1);
}

int ts = 0;
int updateSpeed = 0;

void loop() {
  updateSpeed = micros() - ts;
  env.set("fps", Value(1000000.0 / updateSpeed));
  env.set("qt", Value(updateSpeed * 0.001));
  ts = micros();

  get_time = 0;
  parse_time = 0;
  run_time = 0;

  RESET_TIMER

  useq::update();

  env.set("perf_get", Value(float(get_time * 0.001)));
  env.set("perf_parse", Value(float(parse_time * 0.001)));
  env.set("perf_run", Value(float(run_time * 0.001)));
  env.set("perf_ts1", Value(float(ts_total * 0.001)));

  // put your main code here, to run repeatedly:
  if (Serial.available()) {
    String cmd = Serial.readString();
    // queue or run now
    //  Serial.println(cmd);
    if (cmd.charAt(0) == '@') {
      // clear the token
      cmd.setCharAt(0, ' ');
      run(cmd, env);
    } else {
      auto parsedCode = ::parse(cmd);
      useq::runQueue.push_back(parsedCode);
    }
    // Serial.println(cmdts * 0.001);
    // Serial.println(test);
    // Serial.println("complete");
    // Serial.println(env.toString(env));
    // Serial.println(updateSpeed);
  }
  readRotaryEnc();
}

#endif // USEQ_H_
