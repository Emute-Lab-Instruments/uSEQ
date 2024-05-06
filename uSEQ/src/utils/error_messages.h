#ifndef ERROR_MESSAGES_H_
#define ERROR_MESSAGES_H_

////////////////////////////////////////////////////////////////////////////////
/// ERROR MESSAGES /////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Ensure that PROGMEM is defined properly
#ifndef PROGMEM
#define PROGMEM
#endif

#include "string.h"

// TODO restore this?
/* #define RO_STRING(x, y) const char PROGMEM x[] = y; */

extern String TOO_FEW_ARGS;
extern String TOO_MANY_ARGS;
extern String INVALID_ARGUMENT;
extern String MISMATCHED_TYPES;
extern String CALL_NON_FUNCTION;
extern String UNKNOWN_ERROR;
extern String INVALID_LAMBDA;
extern String INVALID_BIN_OP;
extern String INVALID_ORDER;
extern String BAD_CAST;
extern String ATOM_NOT_DEFINED;
extern String EVAL_EMPTY_LIST;
extern String INTERNAL_ERROR;
extern String INDEX_OUT_OF_RANGE;
extern String MALFORMED_PROGRAM;

#endif // ERROR_MESSAGES_H_
