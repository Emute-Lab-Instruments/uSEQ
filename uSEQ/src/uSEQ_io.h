#ifndef USEQ_IO_H_
#define USEQ_IO_H_

////////////////////////////////////////////////////////////////////////////////
/// I/O RELATED LISP FUNCTION DECLARATIONS
/// This header contains LISP function declarations for I/O operations
/// that need to be included within the uSEQ class definition.
////////////////////////////////////////////////////////////////////////////////

// Analog output functions (a1-a8)
LISP_FUNC_DECL(useq_a1);
LISP_FUNC_DECL(useq_a2);
LISP_FUNC_DECL(useq_a3);
LISP_FUNC_DECL(useq_a4);
LISP_FUNC_DECL(useq_a5);
LISP_FUNC_DECL(useq_a6);
LISP_FUNC_DECL(useq_a7);
LISP_FUNC_DECL(useq_a8);

// Digital output functions (d1-d8)
LISP_FUNC_DECL(useq_d1);
LISP_FUNC_DECL(useq_d2);
LISP_FUNC_DECL(useq_d3);
LISP_FUNC_DECL(useq_d4);
LISP_FUNC_DECL(useq_d5);
LISP_FUNC_DECL(useq_d6);
LISP_FUNC_DECL(useq_d7);
LISP_FUNC_DECL(useq_d8);

// Serial output functions (s1-s8)
LISP_FUNC_DECL(useq_s1);
LISP_FUNC_DECL(useq_s2);
LISP_FUNC_DECL(useq_s3);
LISP_FUNC_DECL(useq_s4);
LISP_FUNC_DECL(useq_s5);
LISP_FUNC_DECL(useq_s6);
LISP_FUNC_DECL(useq_s7);
LISP_FUNC_DECL(useq_s8);

// Output value getter functions
LISP_FUNC_DECL(useq_get_a1);
LISP_FUNC_DECL(useq_get_a2);
LISP_FUNC_DECL(useq_get_a3);
LISP_FUNC_DECL(useq_get_a4);
LISP_FUNC_DECL(useq_get_a5);
LISP_FUNC_DECL(useq_get_a6);
LISP_FUNC_DECL(useq_get_a7);
LISP_FUNC_DECL(useq_get_a8);

LISP_FUNC_DECL(useq_get_d1);
LISP_FUNC_DECL(useq_get_d2);
LISP_FUNC_DECL(useq_get_d3);
LISP_FUNC_DECL(useq_get_d4);
LISP_FUNC_DECL(useq_get_d5);
LISP_FUNC_DECL(useq_get_d6);
LISP_FUNC_DECL(useq_get_d7);
LISP_FUNC_DECL(useq_get_d8);

// Input functions
LISP_FUNC_DECL(useq_in1);
LISP_FUNC_DECL(useq_in2);
LISP_FUNC_DECL(useq_ain1);
LISP_FUNC_DECL(useq_ain2);

// Clock and timing I/O
LISP_FUNC_DECL(useq_set_clock_internal);
LISP_FUNC_DECL(useq_set_clock_external);
LISP_FUNC_DECL(useq_get_clock_source);
LISP_FUNC_DECL(useq_reset_internal_clock);
LISP_FUNC_DECL(useq_reset_external_clock_tracking);
LISP_FUNC_DECL(useq_get_input_bpm);

// Control surface functions
LISP_FUNC_DECL(useq_q0);

#ifdef ARDUINO
// Arduino-specific I/O
LISP_FUNC_DECL(ard_useqaw);
LISP_FUNC_DECL(ard_useqdw);
LISP_FUNC_DECL(ard_aw);
LISP_FUNC_DECL(ard_dw);
#else
// Desktop build versions
LISP_FUNC_DECL(ard_useqaw);
LISP_FUNC_DECL(ard_useqdw);
LISP_FUNC_DECL(ard_aw);
LISP_FUNC_DECL(ard_dw);
#endif

#ifdef MIDIOUT
// MIDI output
LISP_FUNC_DECL(useq_mdo);
#endif

#endif // USEQ_IO_H_