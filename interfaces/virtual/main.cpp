#define PROGMEM
#define PICO_NO_HARDWARE 1
#define NO_ETL
//#include <Embedded_Template_Library.h> // Mandatory for Arduino IDE only
#include "String.h"

#include <iostream>
#include <cmath>
#include <chrono>

#include <SerialPort.h>
#include <SerialStream.h>
#include <csignal>

using namespace LibSerial ;

//next: https://libserial.readthedocs.io/en/latest/tutorial.html
//set up virtual serial using socat


using std::byte;



struct DummySerial {
    DummySerial(std::string port="") {
        if (port != "") {
            tty.Open(port);
        }
    }
    SerialPort tty;

    std::string buf="";

    void print(String s) {
        std::cout << s.c_str();
    }
    void println(String s) {
        std::cout << s.c_str()<< std::endl;
    }
    void println(int x) {
        std::cout << x << std::endl;
    }
    void println() {
        std::cout << std::endl;
    }
    void write(byte x) {}
    void write(int x) {}
    void setRX(int x) {}
    void setTX(int x) {}
    void begin(int x) {}
    void setTimeout(int x){}
    bool available() {return tty.IsDataAvailable();}
    String readString() {
        char ch;
        std::string s;
        while (tty.IsDataAvailable()) {
            try {
                tty.ReadByte(ch, 1);
                if (ch==10) {
                    break;
                }else{
                    std::cout << "ch: " << int(ch) << std::endl;
                    s+=ch;
                }
            }
            catch (ReadTimeout) {
            }
        }
        return String(s.c_str());
    }
};
DummySerial Serial("/dev/pts/3");
DummySerial Serial1;

uint64_t millis()
{
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return ms;
}

// Get time stamp in microseconds.
uint64_t micros()
{
    uint64_t us = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    return us;
}
long int random(long int low, long int high) {
    return low + (std::rand() % high);
}


struct PIO {};
PIO pio0;
PIO pio1;
void pio_sm_set_enabled(PIO pio, int sm, bool v) {}
void pio_sm_put_blocking(PIO pio, int sm, int period) {}
int pio_encode_pull(bool x, bool y) {
    return 0;
}
void pio_sm_exec(PIO pio, int sm, int a) {}
enum pio_src_dest {
    pio_pins = 0u,
    pio_x = 1u,
    pio_y = 2u,
    pio_null = 3u,
    pio_pindirs = 4u ,
    pio_exec_mov = 4u,
    pio_status = 5u,
    pio_pc = 5u,
    pio_isr = 6u,
    pio_osr = 7u ,
    pio_exec_out = 7u ,
};
uint pio_encode_out(enum pio_src_dest dest, uint count) {
    return 0;
}
int digitalRead(int pin) {return 0;}
int analogRead(int pin) {return 0;}
void digitalWrite(int pin, int val) {}
void analogWrite(int pin, int val) {}
void pinMode(int pin, int mode) {}
void delay(int d) {}
void delayMicroseconds(int d) {}
float map(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min + 1) / (in_max - in_min) + out_min;
}
void randomSeed(int x) {};

#define LED_BUILTIN 25
#define OUTPUT 1
#define INPUT_PULLUP 0
void analogWriteFreq(int x) {}
void analogWriteResolution(int x) {}
using std::min;
using std::max;

struct Dummyrp2040 {
    int getFreeHeap() {
        return 0;
    }
};

Dummyrp2040 rp2040;

typedef struct {
    uint32_t clkdiv;
    uint32_t execctrl;
    uint32_t shiftctrl;
    uint32_t pinctrl;
} pio_sm_config;

typedef struct pio_program {
    const uint16_t *instructions;
    uint8_t length;
    int8_t origin; // required instruction memory origin or -1
} pio_program_t;


pio_sm_config pio_get_default_sm_config() {
    pio_sm_config c = {0, 0, 0, 0};
    return c;
}

void sm_config_set_wrap(pio_sm_config *c, uint wrap_target, uint wrap) {}
void sm_config_set_sideset_pins(pio_sm_config *c, uint sideset_base) {}
void sm_config_set_sideset(pio_sm_config *c, uint bit_count, bool optional, bool pindirs) {}
void pio_gpio_init(PIO pio, uint pin) {}
void pio_sm_set_consecutive_pindirs(PIO pio, uint sm, uint pin_base, uint pin_count, bool is_out){}
void pio_sm_init(PIO pio, uint sm, uint initial_pc, const pio_sm_config *config){}
uint pio_add_program(PIO pio, const pio_program_t *program){return 0;}


void readInputs();

//#include "../../LispLibrary.h"
//#include "../../MAFilter.hpp"
//#include "../../pinmap.h"
//#include "../../piopwm.h"
#include "../../uSEQ.ino"

//SerialPort testTTY("/dev/pts/3");

void my_handler(int s){
    std::cout << "Caught signal";
//    testTTY.Close();
    exit(1);

}



int main() {
    std::signal (SIGINT,my_handler);
    std::signal (SIGTERM,my_handler);
    std::cout << "Hello, World!" << std::endl;
//    String cmd;
//    std::cin >> cmd;
//    std::cout << cmd << std::endl;
    setup();
    while(1) {
        loop();
    }
    //test serial port
//    while(1) {
//        char ch;
//        if (testTTY.IsDataAvailable()) {
//            try {
//                testTTY.ReadByte(ch, 2);
//            }
//            catch (ReadTimeout) {
//
//            }
//            std::cout << "ch: " << int(ch) << std::endl;
//        }
//    }
    return 0;
}
