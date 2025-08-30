Executive Summary

- Core interpreter is well-structured and already well-tested; tests pass quickly
on desktop via Meson.
- Hardware, timing, DSP, and persistence are tightly coupled to application logic,
blocking near-100% testability.
- A layered, ports-and-adapters architecture with dependency injection will let
us test almost all behavior deterministically without runtime overhead, by using
compile-time selection and lightweight indirection in non-hot paths.

Strengths

- Tests: Catch2 tests cover parser, value semantics, environment, and interpreter
APIs; all green in build.
- Build: Meson project with a clean standalone target, sensible warnings, fast
desktop workflow.
- Separation: Interpreter modules are largely hardware-agnostic; OutputManager
begins to reduce duplication.
- Utilities: Arduino compatibility shims enable desktop builds; guard macros
isolate embedded paths.

Key Risks and Smells

- Global/static state: uSEQ::instance, Interpreter static flags, global error
queues create hidden coupling and flakiness risk.
- Monolithic types: Value mixes many concerns with a manual tag + union; unsafe by
construction and harder to reason about.
- Side effects inside domain: Time, I/O, DSP, and flash mixed into interpreter/
uSEQ flow; no clear domain/application/infrastructure separation.
- Ad-hoc macros: Conditional compilation scattered across code; String abstraction
complexity; mixed naming and boundaries.
- Error handling: Value-as-error pattern without a uniform, typed error channel;
easy to misuse and hard to assert in tests.
- Limited verification: No property/fuzz coverage for parser; no contract-level
tests for scheduling/time logic; no coverage or sanitizers in CI.

Target Architecture

- Layers:
    - Core Domain: parser, Value, environment, evaluator, scheduling logic, and
OutputManager policy.
    - Ports (interfaces):
    - Clock: monotonic time source, sleep/yield.
    - GPIO/ADC/PWM: digital, analog I/O.
    - I2C bus: message send/receive, addressing.
    - DSP: node graph lifecycle, parameter set/get, queues.
    - Storage: key–value or block device-like persistence.
    - Logger: leveled logger and user-message sink.
- Adapters:
    - Pico/Arduino adapters implementing ports.
    - Desktop mock/fake adapters for tests.
- Construction:
    - Dependency injection at construction (constructor/setter) for ports.
    - No globals or singletons; pass context explicitly.
- Performance:
    - Default to virtual interfaces in non-hot paths.
    - For hot loops, use CRTP or function-pointers bound once at construction, or
final vtables to enable devirtualization.
    - Keep embedded -Os with link-time optimization; keep indirections off the DSP
inner loops.

Surgical Refactors (Minimal runtime impact)

- Value safety:
    - Introduce a thin, compile-time checked tagged union wrapper or std::variant
for desktop builds behind a typedef; keep current struct on embedded if flash/
ram limited.
    - Centralize constructors and accessors with assertions (debug builds) and
total functions returning expected error Value types.
- Time determinism:
    - Extract a IClock port used by ModuLisp/uSEQ scheduling; remove static Value
t mutation in evaluation; compute from injected time in evaluation context.
- Logging:
    - Define ILogger port; strip global queues; use a simple ring buffer backing
to enable assertions in tests.
- Output and I/O:
    - Keep OutputManager as the policy layer; route hardware writes through IIo
port; OutputManager remains pure aside from calling the port.
- DSP:
    - Wrap DSP engine behind IDSPEngine port; create a stub/mock implementing same
queue semantics for tests.
- Persistence:
    - Define IStorage with minimal primitives used by flash code; implement
RAM-backed storage for tests; verify serialization logic deterministically.
- I2C:
    - II2CBus abstraction; remove global buffers; use an explicit message type;
allow end-to-end “multi-module” simulation in tests with an in-memory bus.

Correctness Upgrades

- Contracts and invariants:
    - Guard Value operations with preconditions; fail-fast in debug via
assertions; keep release behavior unchanged.
    - Validate argument predicates in builtins uniformly via helpers that surface
typed errors; test both success and error paths.
- Deterministic randomness:
    - Extract RNG provider; seed in tests; guarantee repeatability.
- Time units:
    - Introduce strong types for time (microseconds/seconds) or a thin wrapper to
prevent unit mistakes; conversion helpers; tested thoroughly.
- Parser resilience:
    - Add property and fuzz tests (desktop only) to prove round-trip and non-crash
invariants; run sanitizers.

Testing Strategy

- Unit tests:
    - Domain: Value arithmetic/ordering/metadata, parser tokens, interpreter
evaluation of special forms, schedulers’ step logic.
    - Ports: mocks verifying call sequences and edge cases; golden tests for
OutputManager mapping.
    - Builtins: table-driven tests for each with bad-arg coverage.
- Property tests:
    - Parser: round-trip to_lisp_src(parse(x)) = normalize(x) for accepted subset;
no crashes on random strings; rejection tests.
    - Value algebra: commutativity, associativity where applicable, monotonicity
predicates for metadata where defined.
- Integration tests:
    - End-to-end: deterministic time sequence → interpreter eval → OutputManager
states → mocked I/O calls.
    - I2C: two uSEQ instances on a mock bus exchange messages; assert protocol
correctness.
    - DSP: create/test node graphs on mock DSP with predictable outputs.
- Fuzzing:
    - libFuzzer/afl-like harness for uLispParser::parse and evaluator on desktop.
Run with UBSan/ASan in CI nightly.
- Coverage and mutation:
    - Enable Meson b_coverage; generate lcov/html; track in CI.
    - Add mutation testing (e.g., Mull) for critical algebra and parser components
to harden tests.

Build/CI Tooling

- Meson options:
    - option('hw', type: 'feature', value: 'disabled') to exclude hardware
adapters in desktop.
    - option('sanitizers', type: 'boolean', value: true) for ASan/UBSan builds
on desktop.
    - option('coverage', type: 'boolean', value: false) to toggle b_coverage.
- Targets:
    - libuseq_core: pure domain library.
    - libuseq_ports: interface headers only.
    - libuseq_adapters_hw: Pico/Arduino adapters (excluded in desktop tests).
    - libuseq_adapters_mock: mock adapters for tests.
    - standalone links core + mock in desktop mode.
- CI:
    - GitHub Actions: matrix gcc/clang, debug/release, sanitizers on, coverage,
artifact upload of coverage and failing seeds.
    - clang-tidy and cppcheck runs; clang-format check (repo already has
_clang-format).
- Pre-commit:
    - Format, tidy, include-what-you-use, header guards consistency,
forbidden-constructs (raw new/delete).

Concrete Migration Plan

- Phase 0: Baseline (1–2 days)
    - Add CI with build + tests + sanitizers + coverage.
    - Add Meson options for coverage/sanitizers; keep default fast path unchanged.
- Phase 1: Ports & DI (1–1.5 weeks)
    - Define ports: IClock, IIo, II2CBus, IDSPEngine, IStorage, ILogger.
    - Add constructor injection to uSEQ and ModuLispInterpreter (with default null
adapters in desktop).
    - Move direct calls behind adapters; keep behavior identical.
- Phase 2: Domain cleanup (1 week)
    - Value safety: centralize construction/access; introduce debug-asserts;
optional std::variant typedef on desktop.
    - Extract RNG and time source; remove static t write-paths; compute from
injected clock in eval context.
    - Uniform error helpers; tests for error variants across builtins.
- Phase 3: Test buildout (1 week)
    - Add property tests for parser; fuzz harness for parser and eval.
    - Add integration tests: time-stepped scheduling, OutputManager mapping, I/O
call verification.
    - Add mocks for I2C and DSP; write initial contract tests.
- Phase 4: Hardware-adjacent refactor (1 week)
    - Isolate flash and I2C in adapters; provide RAM-backed storage; test load/
save cycles deterministically.
    - Optional: simulate jitter/latency in mock clock/bus to test robustness
boundaries.
- Phase 5: Hardening (ongoing)
    - Mutation testing for parser/value algebra.
    - Perf profiling of adapter indirection; apply CRTP/final to hot paths if
needed.
    - Documentation: architecture overview, port contracts, testing guide.

Low-Risk Quick Wins

- Turn on sanitizers in desktop CI to catch UB early.
- Add coverage target and track trend; enforce minimum on changed lines.
- Expand tests around error reporting in log.h/error helpers.
- Normalize String use: alias a single string type per target; reduce conversions.
- Expand OutputManager tests to pin-map coverage and bounds behavior.

File-Level Pointers

- Strong candidates for ports:
    - uSEQ/src/uSEQ_io.cpp, uSEQ/src/uSEQ_i2c.cpp, flash code in uSEQ/src/
uSEQ_flash.cpp.
    - Timing in uSEQ/src/modulisp/modulisp.cpp and uSEQ/src/modulisp/lisp/
interpreter.*.
- Strong candidates for domain cleanups:
    - uSEQ/src/modulisp/lisp/value.* (type safety, invariants, errors).
    - uSEQ/src/modulisp/lisp/parser.* (property/fuzz tests).
- Duplication reduction:
    - uSEQ/src/uSEQ/output_manager.* already a good start; extend to input
handling similarly.

Performance Notes

- Keep ports out of tight inner loops; call adapters at the boundary (per tick/
write), not per sample.
- Use final and non-virtual inline helpers where the compiler can devirtualize; or
CRTP for hot code paths.
- Maintain current flags; optionally enable LTO for release and keep debug builds
instrumented.

If you want, I can:

- Add Meson options for sanitizers/coverage and wire a basic GitHub Actions CI
file.
- Sketch the port interfaces and replace the first concrete calls (clock +
logging) to demonstrate the pattern.

user
Can you implement a long, serialised, numbered TODO list that will get us there
step by step? Include as much info as you can, such as source code locations or
code snippets etc, anything to make the life of the AI coding agent that will
implement them as easy and straightforward as possible

codex
Roadmap Overview

- Goal: Near-100% testability via ports-and-adapters, dependency injection, and
deterministic time/RNG, with no runtime regressions.
- Strategy: Add interfaces (ports), provide mock adapters, refactor uSEQ/ModuLisp
to depend on ports, incrementally replace direct hardware calls, and expand tests/
CI.

Serialised TODOs

-
    1. Establish Meson options for testing
    - Add toggles to meson.build.
    - Add: option('sanitizers', type: 'boolean', value: true), option('coverage',
type: 'boolean', value: false), option('hw', type: 'feature', value: 'disabled').
    - Wire into build:
    - If `get_option('sanitizers')`, add `-fsanitize=address,undefined
-fno-omit-frame-pointer`.
    - If `get_option('coverage')`, add `-O0 -g --coverage` and make coverage
target.
-
Paths: meson.build.
-
    2. Create core library targets
    - Split targets to libraries:
    - `libuseq_core`: all files in current `common_src` except Arduino-specific.
    - `libuseq_adapters_mock`: new mocks in `uSEQ/src/ports/mocks`.
    - `libuseq_adapters_hw`: ARDUINO/Pico adapters compiled only when
`get_option('hw').enabled()`.
-
Paths: meson.build, add library() calls and change executable() sources to link
libs.
-
    3. Introduce ports directory
    - Create uSEQ/src/ports/ with interface headers:
    - `IClock.h`, `ILogger.h`, `IIo.h`, `II2CBus.h`, `IDSPEngine.h`, `IStorage.h`,
`IRng.h`.
- Keep interfaces thin with pure virtual methods and no STL where embedded cost
matters.
- Example uSEQ/src/ports/IClock.h:
    - `struct IClock { virtual ~IClock()=default; virtual uint64_t micros()=0;
virtual uint64_t millis()=0; };`

-
    4. Add mock adapters
    - Create uSEQ/src/ports/mocks/ with:
    - `MockClock`, `MockLogger`, `MockIo`, `MockI2CBus`, `MockDSPEngine`,
`MockStorage`, `MockRng`.
-
Each with simple in-memory behavior and minimal dependencies. E.g. MockClock with
settable time.
-
    5. Add hardware adapters (skeletons)
    - Create uSEQ/src/ports/hw/ with:
    - `ArduinoClock`, `ArduinoLogger` (mapping to println/debug), `ArduinoIo`
(pinMode/digitalWrite/analogRead etc.), `ArduinoI2CBus`, `ArduinoStorage` (flash),
`PicoDSPEngine` if applicable.
-
Initially stub if time-constrained; wire later.
-
    6. Wire DI into ModuLispInterpreter
    - Modify uSEQ/src/modulisp/modulisp.h:
    - Add members: `IClock* clock; ILogger* logger; IRng* rng;` (raw pointers
owned by an external owner by default).
    - Add constructor taking optional pointers; default to nullptr.
-
Modify ModuLispInterpreter::update_time() and users of micros()/millis() to call
clock->micros() when clock != nullptr; fallback to existing functions otherwise.
-
Provide a setter for test convenience if needed.
-
    7. Wire DI into uSEQ
    - Modify uSEQ/src/uSEQ.h and uSEQ/src/uSEQ.cpp:
    - Add members: `IIo* io; II2CBus* i2c; IDSPEngine* dsp; IStorage* storage;`
and reuse `clock/logger/rng` from base or hold here.
    - Add constructor: `explicit uSEQ(IClock* c=nullptr, ILogger* l=nullptr, IRng*
r=nullptr, IIo* io=nullptr, II2CBus* b=nullptr, IDSPEngine* d=nullptr, IStorage*
s=nullptr);`
-
Update factory/initializers to set default adapters on embedded (Arduino) paths.
-
    8. Replace direct logging calls
    - Abstract println, debug, report_error behind ILogger.
    - In uSEQ/src/utils/log.h/.cpp, implement a LoggerAdapter that forwards to
existing functions; add ILogger methods: info, warn, error, debug.
    - In ModuLisp/uSEQ, call logger? logger->error(msg) : report_error(msg);.
-
    9. Replace direct time calls
    - Search: rg -n "micros\\(|millis\\(" uSEQ/src.
    - In ModuLisp and uSEQ, replace with clock->micros() if non-null; preserve
macro guarded fallback for Arduino.
-
   10. Introduce RNG port
    - Add IRng { virtual uint32_t next()=0; virtual void seed(uint32_t)=0; virtual
double next01()=0; };
    - Replace ModuLispInterpreter::m_random_seed and random helpers to use rng if
provided; fallback to current simple_hashing_function for backward compatibility.
-
   11. Refactor OutputManager to delegate I/O
    - OutputManager should remain policy-only but it touches env AST arrays.
    - Create a thin boundary in uSEQ update paths that calls io:
    - `void analog_write_with_led(int, CONTINUOUS_OUTPUT_VALUE_TYPE)` route to
`io->analogWrite(pin, value)` and then LED; LED write also via `io->digitalWrite`.
    - `void digital_write_with_led(int, ...)` route to `io->digitalWrite(...)`.
    - `void serial_write(int, ...)` define on `IIo` or a separate `ISerial` port;
for now add method `serialWrite(channel, value)`.
-
Paths: uSEQ/src/uSEQ.h, uSEQ/src/uSEQ.cpp, uSEQ/src/uSEQ/output_manager.*.
-
   12. Extract I/O ops into IIo
    - Define IIo with:
    - `void pinMode(uint8_t, uint8_t)`, `void digitalWrite(uint8_t,uint8_t)`, `int
digitalRead(uint8_t)`, `void analogWrite(uint8_t,int)`, `int analogRead(uint8_t)`,
`void serialWrite(uint8_t, SERIAL_OUTPUT_VALUE_TYPE)`, optional PWM config.
-
Implement MockIo to record calls (vectors of structs with timestamp and params)
for assertions in tests.
-
   13. Introduce I2C message bus
    - Define struct I2CMessage { uint8_t src; uint8_t dst; std::vector<uint8_t>
payload; };
    - Interface: virtual bool send(const I2CMessage&)=0; virtual bool
available(uint8_t addr)=0; virtual bool receive(uint8_t addr, I2CMessage& out)=0;.
    - Implement MockI2CBus with per-address queues; optional latency simulation.
    - Refactor uSEQ/src/uSEQ_i2c.cpp to use i2c port.
-
   14. Extract storage (flash) operations
    - Interface: bool write(uint32_t offset, const uint8_t* data, size_t len),
bool read(uint32_t offset, uint8_t* data, size_t len), bool erase(uint32_t offset,
size_t len).
    - Implement MockStorage with std::vector<uint8_t> buffer of fixed size.
    - Refactor uSEQ/src/uSEQ_flash.cpp functions (write/load env, info) to use
storage.
-
   15. DSP engine port
    - Interface minimal for current usage in uSEQ/src/uSEQ.h DSP section:
    - lifecycle: `bool start()`, `void stop()`, `size_t create(const String&
type)`, `bool connect(size_t out, size_t in)`, `bool disconnect(...)`, `bool
setParam(size_t id, const String& key, float val)`, queue get/set.
-
Implement MockDSPEngine with predictable behavior, last-value memory, and queue
maps.
-
Update DSP LISP builtins to call dsp port, not hardware-specific code.
-
   16. Evaluation context for time
    - Add a light struct EvalContext { double time_s; }.
    - Modify ModuLispInterpreter::eval_at_time(Value&, Environment&, double)
to construct context and pass through to evaluation path (you already have this
function; ensure it is used consistently by scheduling).
    - Avoid modifying static Value::t globally; pass through env binding for t
when evaluating (bind t atom to a Value constructed from time_s in a child env).
    - Transitional: Keep current global until complete; guard with macro
USE_EVAL_CONTEXT_TIME.
-
   17. Strengthen Value invariants (phase A)
    - Add internal asserts in Value::as_* methods to check type matches.
    - Ensure error() produces a distinguishable type; add helpers like bool
is_error() const;.
    - Expand tests for error paths in test/test_value_api.cpp.
-
   18. Variant-based Value for desktop (phase B, optional)
    - Guard: #if USE_SAFE_VALUE_DESKTOP use std::variant<int, double, String,
vector<Value>, ...> and a wrapper class maintaining same API; keep union-based
for embedded.
    - Keep API identical to avoid ripple.
    - Provide conversion layer confined to value.h/.cpp.
-
   19. Uniform error helpers
    - Add functions in utils/log.{h,cpp}:
    - `Value make_arg_error(const String& fn, int idx, const Value& got, const
String& expected);`
    - Use in builtins to return typed error Value; tests assert both error type
and message via logger.
-
Replace scattered error reporting.
-
   20. Deterministic randomness in builtins
    - Update useq_random, useq_index_rand to use rng->next01() if available.
    - Add seed LISP builtin to control RNG for tests.
    - Update tests in test/test_modulisp_api.cpp to seed and assert deterministic
behavior.
-
   21. Replace direct Arduino includes in utils/string.h
    - Ensure desktop build uses Extended_STD_String consistently; consolidate
Arduino String compatibility via a single typedef to reduce branchy code.
    - Add unit tests proving that String ops behave identically on desktop.
-
   22. Expand OutputManager tests
    - Add test/test_output_manager.cpp:
    - Use `MockIo`, build a `uSEQ` with mocks, set expressions for `a1/d1/s1`,
run minimal update, assert `MockIo` recorded expected writes and getters return
expected values.
-
Meson: add executable and register test.
-
   23. Parser property tests
    - Add test/test_parser_props.cpp:
    - Generate random tokens/strings; ensure `parse` doesn’t crash; for simple
grammar subset, assert `to_lisp_src(parse(x))` equals normalized x.
    - Add fuzz loops within time bounds.

-
   24. Fuzz harness targets (desktop only)
    - Add test/fuzz_parser.cpp harness for libFuzzer (only built when sanitizers
on).
    - Meson custom target to link with -fsanitize=fuzzer when clang is available.
-
   25. Sanitizers in CI
    - Add GitHub Actions workflow .github/workflows/ci.yml:
    - Matrix: ubuntu-latest, gcc/clang, `sanitizers=true`, `coverage=true` for
one job.
    - Steps: setup deps, `meson setup build -Dsanitizers=true -Dcoverage=true`,
`meson test -C build`, upload coverage (`gcovr` or `lcov`).
-
Keep Arduino/HW builds as separate job (optional) without running tests.
-
   26. Coverage target
    - Add Meson run_target('coverage', command: ['gcovr', '--xml', ...]).
    - Document in README how to run coverage locally.
-
   27. Lint and formatting
    - Add clang-tidy config and pre-commit hooks (optional if team uses).
    - CI job to run clang-tidy on uSEQ/src/modulisp/lisp/*.{h,cpp} and new ports.
-
   28. I2C integration test
    - Add test/test_i2c_integration.cpp:
    - Two `uSEQ` instances with shared `MockI2CBus`; send a serialized program/
message; assert other receives and evaluates; verify side effect via `MockIo`.
-
Define a simple message format for tests (e.g., UTF-8 command bytes).
-
   29. Storage tests
    - Add test/test_storage_env.cpp:
    - Use `MockStorage`; call functions equivalent to `useq_write_flash_env`/
`useq_load_flash_env`; assert bytes and deserialized env equivalence.

-
   30. DSP tests
    - Add test/test_dsp_contract.cpp with MockDSPEngine:
    - Create nodes, connect, set params, enqueue/dequeue data; assert consistent
state updates and last values.

-
   31. Remove static globals incrementally
    - uSEQ::instance: add set_global_instance(uSEQ*) only for ISR/Arduino callback
bridging; set in uSEQ::init(); in desktop tests avoid using ISR bridging.
    - Replace remaining static flags in Interpreter with members; ensure no
cross-test leakage.
-
   32. Time unit safety (optional)
    - Define lightweight struct Micros { uint64_t v; }; struct Seconds { double
v; }; with conversion helpers; use in new clock code paths.
    - Not necessary to retrofit everywhere; use at boundaries and tests.
-
   33. Document architecture
    - Add docs/ARCHITECTURE.md: ports and adapters diagram, ownership, lifetimes,
threading expectations.
    - Add docs/TESTING.md: how to run tests, fuzzers, coverage; how to write new
port mocks.
-
   34. Maintain performance
    - Annotate hot methods with final on adapters to allow devirtualization when
statically known.
    - For critical loops (e.g., signal update), cache adapter pointers in local
references; avoid per-sample virtual calls; keep I/O calls at the per-tick
boundary.
-
   35. Harden builtins argument checking
    - Create helpers like require_num_args(args, 2, "add"),
require_all_numeric(args, "add").
    - Replace manual checks across builtin files (uSEQ/src/modulisp/lisp/
generated_builtins.cpp and other builtin sources).
    - Add tests for wrong-arg errors per builtin in test/
test_generated_builtins.cpp.
-
   36. Normalize Value::t usage
    - Bind t via env in eval_at_time and scheduling; add tests that check that
time-dependent builtins produce expected values given EvalContext.time.
    - Keep static Value::t as deprecated alias for now; log a warning in debug
builds if used directly.
-
   37. Add golden tests for scheduling
    - test/test_scheduling.cpp: With MockClock, advance time deterministically;
verify scheduled ASTs fire the correct number of times per phase; assert outputs
updated.
-
   38. Update test meson definitions
    - test/meson.build: add new test sources (test_output_manager.cpp,
test_parser_props.cpp, test_i2c_integration.cpp, test_storage_env.cpp,
test_dsp_contract.cpp, test_scheduling.cpp).
    - Ensure they link libuseq_core + libuseq_adapters_mock.
-
   39. Clean up Arduino shims
    - uSEQ/src/utils/common.h/.cpp: ensure no name collisions with ports; keep
as-is for ARDUINO builds but desktop paths should prefer ports.
-
   40. Add error and logger contract tests
    - test/test_logging.cpp: Inject MockLogger, run operations that error; assert
captured messages and types; ensures error channel remains testable.
-
   41. Track open TODOs in code
    - Add // TODO(testability): comments with ticket IDs to ease tracking, but
avoid blocking merges on deep refactors; keep PRs small and focused.
-
   42. Staged merge plan
    - PR1: Meson options + CI + core ports + clock/logger wiring; no functional
changes.
    - PR2: RNG + OutputManager route through IIo; add OM tests.
    - PR3: I2C bus port + integration tests.
    - PR4: Storage port + tests.
    - PR5: DSP port + tests.
    - PR6: EvalContext time + scheduling tests.
    - PR7: Error helpers + builtin arg validation tests.
    - PR8: Parser props + fuzz harness.
    - PR9+: Optional Value variant on desktop.

Code Snippets

- Ports interfaces
    - uSEQ/src/ports/IClock.h
    - `#pragma once`
    - `struct IClock { virtual ~IClock()=default; virtual uint64_t micros()=0;
virtual uint64_t millis()=0; };`
- uSEQ/src/ports/ILogger.h
    - `struct ILogger { virtual ~ILogger()=default; virtual void info(const
String&)=0; virtual void warn(const String&)=0; virtual void error(const
String&)=0; virtual void debug(const String&)=0; };`
- uSEQ/src/ports/IIo.h
    - `struct IIo { virtual ~IIo()=default; virtual void
pinMode(uint8_t,uint8_t)=0; virtual void digitalWrite(uint8_t,uint8_t)=0;
virtual int digitalRead(uint8_t)=0; virtual void analogWrite(uint8_t,int)=0;
virtual int analogRead(uint8_t)=0; virtual void serialWrite(uint8_t,
SERIAL_OUTPUT_VALUE_TYPE)=0; };`
- uSEQ/src/ports/II2CBus.h
    - `struct I2CMessage { uint8_t src; uint8_t dst; std::vector<uint8_t>
payload; };`
    - `struct II2CBus { virtual ~II2CBus()=default; virtual bool send(const
I2CMessage&)=0; virtual bool available(uint8_t addr)=0; virtual bool
receive(uint8_t addr, I2CMessage& out)=0; };`
- uSEQ/src/ports/IStorage.h
    - `struct IStorage { virtual ~IStorage()=default; virtual bool write(uint32_t
off, const uint8_t* data, size_t len)=0; virtual bool read(uint32_t off, uint8_t*
data, size_t len)=0; virtual bool erase(uint32_t off, size_t len)=0; };`
- uSEQ/src/ports/IRng.h
    - `struct IRng { virtual ~IRng()=default; virtual void seed(uint32_t)=0;
virtual uint32_t next()=0; virtual double next01()=0; };`
- uSEQ/src/ports/IDSPEngine.h
    - Minimal subset based on current DSP usage in `uSEQ/src/uSEQ.h`.

- ModuLispInterpreter constructor
    - ModuLispInterpreter(IClock* c=nullptr, ILogger* l=nullptr, IRng*
r=nullptr) : clock(c), logger(l), rng(r) {}
    - In update_time(): auto now_us = clock ? clock->micros() : micros();
    - In update_time(): auto now_us = clock ? clock->micros() : micros();
-
uSEQ constructor
    - uSEQ::uSEQ(IClock* c, ILogger* l, IRng* r, IIo* io_, II2CBus*
bus, IDSPEngine* d, IStorage* s) : ModuLispInterpreter(c,l,r),
m_output_manager(nullptr), io(io_), i2c(bus), dsp(d), storage(s) {}
-
OutputManager usage
    - In analog_write_with_led: if (io) { io->analogWrite(pin, val);
io->digitalWrite(led_pin, val>threshold); }
-
Meson toggles
    - At top-level:
    - `san = get_option('sanitizers') ?
['-fsanitize=address,undefined','-fno-omit-frame-pointer'] : []`
    - `cov = get_option('coverage') ? ['--coverage','-O0','-g'] : []`
    - `add_global_arguments(san + cov, language: 'cpp')`

Notes

- Keep Arduino behavior 1:1; only route calls through adapters.
- Keep PRs small to reduce merge risk; add unit tests alongside each refactor to
lock in behavior.
- Ensure mocks and tests are header-only/simple to avoid new heavy deps.

If you want, I can start by adding the ports directory with the interface headers
and wire IClock + ILogger into ModuLispInterpreter/uSEQ, plus Meson options and a
basic CI YAML to kick off the process.