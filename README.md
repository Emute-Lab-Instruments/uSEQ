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
* Stream waveforms back over USB serial to a computer, and route it to MIDI or OSC, linking the modular with software and external hardware
* Livecoding engine with flexible timing and varied options for creating gate and CV patterns (from basic waveforms to euclidean sequencing) and structuring arrangements


More info from [https://www.emutelabinstruments.co.uk/useq/](https://www.emutelabinstruments.co.uk/useq/)

## Canonical Specs

The firmware/language specs live in [docs/specs/MAIN.md](docs/specs/MAIN.md).
Protocol details live in [docs/specs/wire-protocol.md](docs/specs/wire-protocol.md).
Read [MAP.md](MAP.md) for the repository index and [ALIGNMENT.md](ALIGNMENT.md)
for the dated mission-level gaps.

## Verification

- `nix-shell` enters the declared compiler, conformance, WASM, PlatformIO,
  and RP2040 goal-gate environment. On unattended hosts, use
  `nix-shell --run '<command>'` so PyYAML and the target tools come from the
  same environment.
- `meson setup build && meson compile -C build && meson test -C build` runs
  the native suites.
- `python3 scripts/run_conformance.py --probe build/test/signal_engine_probe --target native`
  runs the shared language corpus against the native probe.
- `scripts/build_wasm.sh` builds and checks the generated interpreter;
  `python3 scripts/run_conformance.py --target wasm` runs the same corpus
  against it; `python3 scripts/wasm_manifest.py verify` verifies its compiler
  record.
- `pio run -e musicthing` establishes target compile, link, and static size;
  it is not a physical-execution result.
- `python3 scripts/rp2040_memory_report.py` reads the exact linked ELF and
  enforces the 96KiB main-heap and 1MiB flash-image headroom gates. Runtime
  heap and stack watermarks remain separate target observations.
- `pio run -e musicthing-observe` builds the release-equivalent telemetry
  image used by simulator and physical-device capacity/endurance runs;
  `musicthing` remains the production image without devtools overhead.
- `python3 scripts/run_rp2040_profile.py` runs the full local RP2040
  acceptance surface: native correctness, constrained-profile conformance,
  capacity and sanitized endurance workloads, both target links, and exact
  ELF memory gates. See
  [docs/testing/rp2040-optimization.md](docs/testing/rp2040-optimization.md).
- `python3 scripts/run_wokwi_rp2040.py` boots that exact observation ELF in
  Wokwi and gates serial responses, target compile and tick timing, runtime
  heap/stack headroom, reset state, retained workload capacity, transactional
  rejection, and externally captured GPIO. It requires `wokwi-cli` and an
  unprinted `WOKWI_CLI_TOKEN`.
- `python3 scripts/run_rp2040_goal_gate.py` composes the complete native,
  exact-link, and simulator grades into one uniquely named candidate evidence
  directory for an optimization loop. Physical-device acceptance remains a
  separate release grade.

## Building the Firmware

The firmware is in the [uSEQ](./uSEQ/) folder.

Build the firmware in Arduino IDE, using the [Earle Philhower Pico core](https://github.com/earlephilhower/arduino-pico).

| Setting  | Value |
| ------------- | ------------- |
| Board  | Generic RP2040  |
| Boot Stage 2  | W25Q080 QSPI /2  |
| Flash Size | 16MB (Sketch 2MB, FS: 14MB) for `musicthing` |
| CPU Speed | 250MHz (Overclock) |
| Optimize | Optimize Even More (-O3) |

Overclock the Pico at 250Mhz and set the optimisation level to -O3.
Other program-card capacities require a matching PlatformIO partition profile;
the default `musicthing` environment is the 16MB card profile.

We welcome pull requests.
