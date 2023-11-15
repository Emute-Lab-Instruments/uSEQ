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
* Inputs
  * Two momentary switches
  * Two toggle switches
  * A rotary encoder + momentary switch
  * Two trigger/gate inputs
* Outputs
  * 6 outputs, configurable as either digital or CV (using PWM)
* Serial port expansion for MIDI IO or connection to other uSEQ modules

## Library

see the [library documentation](docs/useq.md)

## Building the Firmware

The firmware is in the [uSEQ](./useq/) folder.

Build the firmware in Arduino IDE, using the [Earle Philhower Pico core](https://github.com/earlephilhower/arduino-pico).  Overclock the Pico at 250Mhz and set the optimisation level to -O3.

## Building a uSEQ module

Contact @chriskiefer (https://ravenation.club/@luuma) for PCBs


## Developer notes

lisplibrary.py is used to convert LispLibrary.lisp into LispLibrary.h

We welcome pull requests.


