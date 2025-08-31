#include "uSEQ.h"
#include "uSEQ/io_manager.h"
#include "uSEQ/configure.h"
#include "utils.h"
#ifndef ARDUINO
#include "hardware_includes.h"
#endif

#ifdef ENABLE_LED_CONTROL



// LED setup function moved to IOManager

// LED animation now delegates to IOManager
void uSEQ::led_animation()
{
    if (m_io_manager) {
        m_io_manager->led_animation();
    }
}

#endif // ENABLE_LED_CONTROL