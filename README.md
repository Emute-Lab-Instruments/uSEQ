# uSEQ

uSEQ is a livecodeable eurorack module.

There's a paper on it here: 

Kyriakoudis, Dimitris, & Kiefer, Chris. (2023, April 19). uSEQ: A LISPy Modular Sequencer for Eurorack with a Livecodable Microcontroller. 7th International Conference on Live Coding (ICLC2023), Utrecht, The Netherlands. https://doi.org/10.5281/zenodo.7843874

![uSEQ Modules](./docs/img/useqModules.jpg)

uSEQ modules, two panel designs (PCB, Aluminium)

![uSEQ In Use](./docs/img/useqSystem.jpg)

uSEQ modules set up within eurorack performance systems


## Features:

* Open source hardware and software
* Low-cost DIY project
* Live code using a simple LISP language library, from a laptop or mobile device, using serial-over-USB
  * Also programmable from a dedicated cross-platform editor [uSEQ Edit](interfaces/useqedit)
* Inputs
  * Two momentary switches
  * Two toggle switches
  * A rotary encoder + momentary switch
  * Two trigger/gate inputs
* Outputs
  * 6 outputs, configurable (on the PCB and in software) as either gate or CV (using PWM)
  * 13 bit resolution PWM outputs
* Serial port expansion for MIDI IO or connection to other uSEQ modules
* Stream waveforms back over USB serial to a computer, and route it to MIDI or OSC, linking the modular with software and external hardware
* Livecoding engine with flexible timing and varied options for creating gate and CV patterns (from basic waveforms to euclidean sequencing) and structuring arrangements


## Library

see the [library documentation](docs/useq.md)

## Building the Firmware

The firmware is in the [uSEQ](./uSEQ/) folder.

Build the firmware in Arduino IDE, using the [Earle Philhower Pico core](https://github.com/earlephilhower/arduino-pico).  
Settings:

| Setting  | Value |
| ------------- | ------------- |
| Board  | Generic RP2040  |
| Boot Stage 2  | W25Q128JV QSPI /4  |
| Flash Size | 8MB (Sketch 1MB, FS: 7MB) |
| CPU Speed | 250MHz (Overclock) |
| Optimize | Optimize Even More (-O3) |

Overclock the Pico at 250Mhz and set the optimisation level to -O3.

## Building a uSEQ module

Contact @chriskiefer (https://ravenation.club/@luuma) for PCBs


## Developer notes

lisplibrary.py is used to convert LispLibrary.lisp into LispLibrary.h

We welcome pull requests.


