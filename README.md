# uSEQ

<p align="center" width="100%"> 
  <img src="https://www.emutelabinstruments.co.uk/assets/images/useq/useq_front1_small_sq.png" 
    alt="uSEQ Assembled Module" height="300" />
</p>

uSEQ is a [Live Coding](https://en.wikipedia.org/wiki/Live_coding) control sequencer for modular synthesizers and other time-varying signals, developed by [Emute Lab Instruments](https://www.emutelabinstruments.co.uk/). 

It was first released as an open-source research project in 2023, when it was published and presented at the 7th International Conference on Live Coding (ICLC2023) in Utrecht, The Netherlands—you can find the paper [here](https://doi.org/10.5281/zenodo.7843874). 

The original motivation was to explore a novel, cost-effective, and horizontally-scalable way to bring live coding real-time generative sequencing to Eurorack modular synthesizers in a way that would allow us to move away from the laptop—instead of "computer & Eurorack", what about "computer _in_ Eurorack", we thought. One Raspberry Pi Pico, a small Lisp interpreter written in C++, a 3D-printed module panel, and a handful of hand-wired connections later and we had an exciting working prototype.

After the positive reception, useful feedback, and multiple requests for a ready-made version, it was released as a commercial open-source project in the summer of 2024, becoming available both as a beginner-friendly DIY kit and as a fully-assembled unit through major international distributors [Thonk](https://www.thonk.co.uk/shop/emute-useq-kitbags/) and [Signal Sounds](https://www.signalsounds.com/emute-lab-instruments-useq-live-coding-voltage-generator-eurorack-module/) respectively—it's currently being stocked by [retailers around the world](https://www.emutelabinstruments.co.uk/useq-buy/).

## Features

* Open source hardware and software.
* Low-cost, DIY-friendly project.
* Live Coding from a laptop, mobile device, or any device that supports serial-over-USB (including other Eurorack modules!).
* Custom programming language called ModuLisp, bridging the gap between the legendary language Lisp of the 1960s and cutting-edge research in programming language theory and design of the 21st century.
* Livecoding engine with flexible timing and varied options for creating gate and CV patterns (from basic waveforms to euclidean sequencing) and structuring arrangements.
* Stream waveforms back over USB serial to a computer, and route it to MIDI or OSC, linking the modular with software and external hardware.

More info at [https://www.emutelabinstruments.co.uk/useq/](https://www.emutelabinstruments.co.uk/useq/)

## Roadmap

uSEQ is in active development, both as a research and as a commercial project. As of March 2025, a new paper is being published at the upcoming [ICLC2025 in Barcelona](https://iclc.toplap.org/2025/) (accompanied by a live performance), bugfixes and new features are being added to the editor nearly weekly, and a major firmware rewrite is underway to result in a cleaner, more modular, and much more easily extensible codebase upon which the project will keep evolving.

Some of the features we have planned for the near future are:

* Input, output, and control expanders—more encoders for your body to talk to the code, more CV & Gate outs for the code to talk to other modules, and more inputs for other modules to talk to your uSEQ(s). 
* Virtual uSEQs: Standalone, plugin (VST), and browser-based uSEQ versions that can sequence other software or hardware through MIDI and OSC, without the need for a Eurorack system.
* Alternate firmware mode with traditional imperative-style programming and mutation, alongside uSEQ's novel purely-functional and temporally-reactive language semantics.
* Tighter integrations with the existing web-based editor, including dynamic state visualisation, an in-editor snippet library to find and share patches, and more.
* [Language Server Protocol](https://microsoft.github.io/language-server-protocol/) (LSP) support, for easy integration with other editors.

## [Developer notes](docs/dev.md)
## Building the Firmware

The firmware is in the [uSEQ](./uSEQ/) folder.

Build the firmware in Arduino IDE, using the [Earle Philhower Pico core](https://github.com/earlephilhower/arduino-pico).

| Setting  | Value |
| ------------- | ------------- |
| Board  | Generic RP2040  |
| Boot Stage 2  | W25Q080 QSPI /2  |
| Flash Size | 8MB (Sketch 1MB, FS: 7MB) |
| CPU Speed | 250MHz (Overclock) |
| Optimize | Optimize Even More (-O3) |

Overclock the Pico at 250Mhz and set the optimisation level to -O3.

We welcome pull requests.
