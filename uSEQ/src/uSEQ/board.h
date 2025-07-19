#ifndef __BOARD_H
#define __BOARD_H

#ifdef ARDUINO_PICO_MAJOR
#define FAST_FUNC(x) __not_in_flash_func(x)
#define HOST "pico"
#else
#define FAST_FUNC(x) x
#define HOST "unknown"

#endif

#endif