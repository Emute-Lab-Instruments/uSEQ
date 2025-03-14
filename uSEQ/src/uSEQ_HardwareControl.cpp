#include "uSEQ_HardwareControl.h"
#include "uSEQ.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

// These global variables are only defined if the module is enabled
#if USE_NEW_MODULES && USE_HARDWARECONTROL_MODULE
// Global variables
float pdm_y   = 0;
float pdm_err = 0;
float pdm_w   = 0;

/* Hardware control implementations will be migrated in a later stage
They are commented out currently to allow a clean build

#ifdef USEQHARDWARE_1_0
bool timer_callback(repeating_timer_t* mst)
{
    pdm_y   = pdm_w > pdm_err ? 1 : 0;
    pdm_err = pdm_y - pdm_w + pdm_err;
    if (pdm_y == 1)
    {
        digitalWrite(USEQ_PIN_LED_AI1, HIGH);
    }
    else
    {
        digitalWrite(USEQ_PIN_LED_AI1, LOW);
    }

    return true;
}
#endif
*/

// Hardware control implementations
void uSEQ::setup_IO() {}
void uSEQ::setup_outs() {}
void uSEQ::setup_continuous_outs() {}
void uSEQ::setup_discrete_outs() {}
void uSEQ::setup_switches() {}

#ifdef ANALOG_INPUTS
void uSEQ::setup_analog_ins() {}
#endif

void uSEQ::setup_digital_ins() {}

#ifdef USEQHARDWARE_0_2
void uSEQ::setup_rotary_encoder() {}
void uSEQ::read_rotary_encoders() {}
#endif

#ifdef USEQHARDWARE_1_0
void uSEQ::setup_pdm() {}
#endif

#endif // USE_NEW_MODULES && USE_HARDWARECONTROL_MODULE