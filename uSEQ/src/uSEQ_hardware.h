#ifndef USEQ_HARDWARE_H_
#define USEQ_HARDWARE_H_

////////////////////////////////////////////////////////////////////////////////
/// HARDWARE-SPECIFIC FUNCTION DECLARATIONS
/// This header contains hardware-specific LISP function declarations
/// that need to be included within the uSEQ class definition.
////////////////////////////////////////////////////////////////////////////////

#ifdef MUSICTHING
// Music Thing hardware-specific functions
LISP_FUNC_DECL(useq_mt_knob);
LISP_FUNC_DECL(useq_mt_knobx);
LISP_FUNC_DECL(useq_mt_knoby);
LISP_FUNC_DECL(useq_mt_swz);
#endif

#ifdef ARDUINO
// Hardware control functions
LISP_FUNC_DECL(useq_swm);
LISP_FUNC_DECL(useq_swt);
LISP_FUNC_DECL(useq_toggle_pick);
LISP_FUNC_DECL(useq_ssin);
#endif

// Hardware interface functions (cross-platform)
LISP_FUNC_DECL(useq_swr);
LISP_FUNC_DECL(useq_rot);

#endif // USEQ_HARDWARE_H_