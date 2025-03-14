#include "uSEQ_LispFunctions.h"
#include "uSEQ.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_LISPFUNCTIONS_MODULE
// Lisp function implementations

Value uSEQ::useq_eval_at_time(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a1(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a2(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a3(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a4(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a5(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a6(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a7(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_a8(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d1(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d2(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d3(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d4(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d5(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d6(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d7(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_d8(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s1(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s2(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s3(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s4(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s5(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s6(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s7(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_s8(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_slow(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_fast(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_offset_time(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_schedule(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_unschedule(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_setbpm(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_get_input_bpm(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_set_time_sig(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_in1(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_in2(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_ain1(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_ain2(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_set_clock_internal(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_set_clock_external(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_get_clock_source(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_reset_internal_clock(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_reset_external_clock_tracking(LISP_FUNC_ARGS) { return Value::nil(); }
#ifdef MUSICTHING
Value uSEQ::useq_mt_knob(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_mt_knobx(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_mt_knoby(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_mt_swz(LISP_FUNC_ARGS) { return Value::nil(); }
#endif
Value uSEQ::useq_ssin(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_swm(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_swt(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_toggle_select(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_swr(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_rot(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_q0(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_dm(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_gates(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_gatesw(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_trigs(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_euclidean(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_eu(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_ratiotrig(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_ratiostep(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_ratioindex(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_ratiowarp(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_phasor_offset(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_flatten(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_step(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_fromList(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_fromFlattenedList(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_seq(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_flatseq(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_interpolate(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::ard_useqaw(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::ard_useqdw(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_load_flash_info(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_write_flash_info(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_reboot(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_set_my_id(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_get_my_id(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_memory_save(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_memory_restore(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_memory_erase(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_load_flash_env(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_write_flash_env(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_autoload_flash(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_stop_all(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_rewind_logical_time(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_firmware_info(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_report_firmware_info(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_tri(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_i2c_host_start(LISP_FUNC_ARGS) { return Value::nil(); }
Value uSEQ::useq_i2c_send_to(LISP_FUNC_ARGS) { return Value::nil(); }
#ifdef MIDIOUT
Value uSEQ::useq_mdo(LISP_FUNC_ARGS) { return Value::nil(); }
#endif

void uSEQ::init_builtinfuncs() 
{
    // This is a stub function that will be implemented later
}

#endif // USE_NEW_MODULES && USE_LISPFUNCTIONS_MODULE