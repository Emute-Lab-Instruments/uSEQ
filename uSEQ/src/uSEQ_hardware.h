#ifndef USEQ_HARDWARE_H_
#define USEQ_HARDWARE_H_

////////////////////////////////////////////////////////////////////////////////
/// HARDWARE-SPECIFIC FUNCTION DECLARATIONS
/// This header contains hardware-specific LISP function declarations
/// that need to be included within the uSEQ class definition.
////////////////////////////////////////////////////////////////////////////////

#ifdef MUSICTHING
// Music Thing knob functions removed - values now automatically updated as environment variables
// in update_inputs() function. Users access them directly as 'knob', 'knobx', 'knoby', 'swz'.
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

LISP_FUNC_DECL(useq_print_led_info);

// Sample management functions
LISP_FUNC_DECL(useq_list_samples);

#endif // USEQ_HARDWARE_H_