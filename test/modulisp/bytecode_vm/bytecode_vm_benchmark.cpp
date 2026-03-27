// Bytecode VM vs public output-path benchmark harness.
// Build via meson; run manually (not registered as a CI test).
//
//   ninja -C build && ./build/test/bytecode_vm_benchmark

#include "../../../uSEQ/src/modulisp/bytecode_vm.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Benchmark cases
// ---------------------------------------------------------------------------

struct BenchCase
{
    const char* name;
    const char* setup_code; // eval'd once to bind to output
    const char* expr;       // raw expression (for compile)
    int iterations;         // eval cycles
};

static BenchCase cases[] = {
    {"constant", "(a1 42)", "42", 100000},
    {"simple arithmetic", "(a1 (+ 1 2))", "(+ 1 2)", 100000},
    {"temporal beat", "(a1 beat)", "beat", 100000},
    {"nested arithmetic",
     "(a1 (+ (* 2 beat) (- 1 (* 0.5 bar))))",
     "(+ (* 2 beat) (- 1 (* 0.5 bar)))", 50000},
    {"trig chain",
     "(a1 (sin (* 6.28 beat)))",
     "(sin (* 6.28 beat))", 50000},
    {"conditional",
     "(a1 (if (> beat 0.5) (sin beat) (cos beat)))",
     "(if (> beat 0.5) (sin beat) (cos beat))", 50000},
    {"let binding",
     "(a1 (let (x (* beat 2)) (+ (sin x) (cos x))))",
     "(let (x (* beat 2)) (+ (sin x) (cos x)))", 50000},
    {"vector seq",
     "(a1 (seq [0 0.25 0.5 0.75 1.0] beat))",
     "(seq [0 0.25 0.5 0.75 1.0] beat)", 50000},
    {"complex signal",
     "(a1 (clamp (+ (* (sin (* 6.28 (fast 2 beat))) 0.5) 0.5) 0 1))",
     "(clamp (+ (* (sin (* 6.28 (fast 2 beat))) 0.5) 0.5) 0 1)", 25000},
};

static constexpr size_t NUM_CASES = sizeof(cases) / sizeof(cases[0]);

// ---------------------------------------------------------------------------
// Timing helpers
// ---------------------------------------------------------------------------

using Clock = std::chrono::high_resolution_clock;

static double bench_public_output_path(ModuLispInterpreter& interp,
                                       const char* output, int iterations)
{
    auto start   = Clock::now();
    volatile double sink = 0.0;
    for (int i = 0; i < iterations; ++i)
    {
        double sample_time = static_cast<double>(i) / static_cast<double>(iterations);
        bool ok  = false;
        sink     = interp.eval_output_at_time(output, sample_time, &ok);
    }
    auto end = Clock::now();
    (void)sink;
    return std::chrono::duration<double, std::micro>(end - start).count();
}

static double bench_vm(const NumericVmProgram& program, int iterations,
                       double beat_dur, double bar_dur)
{
    TemporalContext ctx;
    ctx.beatDur    = beat_dur;
    ctx.barDur     = bar_dur;
    ctx.phraseDur  = bar_dur * 4.0;
    ctx.sectionDur = bar_dur * 16.0;

    auto start = Clock::now();
    volatile double sink = 0.0;
    for (int i = 0; i < iterations; ++i)
    {
        ctx.t    = static_cast<double>(i) / static_cast<double>(iterations);
        ctx.beat = std::fmod(ctx.t / beat_dur, 1.0);
        ctx.bar  = std::fmod(ctx.t / bar_dur, 1.0);
        ctx.phrase  = std::fmod(ctx.t / ctx.phraseDur, 1.0);
        ctx.section = std::fmod(ctx.t / ctx.sectionDur, 1.0);
        ctx.beatNum = static_cast<int>(ctx.t / beat_dur);
        ctx.barNum  = static_cast<int>(ctx.t / bar_dur);
        ctx.time_since_boot = ctx.t;

        auto result = execute_numeric_program(program, ctx);
        if (result.ok)
            sink = result.value;
    }
    auto end = Clock::now();
    (void)sink;
    return std::chrono::duration<double, std::micro>(end - start).count();
}

// ---------------------------------------------------------------------------
// Correctness verification
// ---------------------------------------------------------------------------

static bool verify_agreement(ModuLispInterpreter& interp,
                             const NumericVmProgram& program,
                             const char* output,
                             double beat_dur, double bar_dur,
                             const char* case_name)
{
    static constexpr int NUM_SAMPLES      = 5;
    static constexpr double TOLERANCE     = 1e-6;
    static const double sample_times[]    = {0.0, 0.1, 0.25, 0.5, 0.9};

    for (int i = 0; i < NUM_SAMPLES; ++i)
    {
        double sample_time = sample_times[i];

        bool tw_ok    = false;
        double tw_val = interp.eval_output_at_time(output, sample_time, &tw_ok);

        TemporalContext ctx;
        ctx.t           = sample_time;
        ctx.beatDur     = beat_dur;
        ctx.barDur      = bar_dur;
        ctx.phraseDur   = bar_dur * 4.0;
        ctx.sectionDur  = bar_dur * 16.0;
        ctx.beat        = std::fmod(sample_time / beat_dur, 1.0);
        ctx.bar         = std::fmod(sample_time / bar_dur, 1.0);
        ctx.phrase      = std::fmod(sample_time / ctx.phraseDur, 1.0);
        ctx.section     = std::fmod(sample_time / ctx.sectionDur, 1.0);
        ctx.beatNum     = static_cast<int>(sample_time / beat_dur);
        ctx.barNum      = static_cast<int>(sample_time / bar_dur);
        ctx.time_since_boot = sample_time;

        auto vm_res = execute_numeric_program(program, ctx);

        if (!tw_ok || !vm_res.ok)
        {
            std::cerr << "  WARNING: " << case_name
                      << " -- evaluation failure at t=" << sample_time
                      << " (public path ok=" << tw_ok
                      << ", vm ok=" << vm_res.ok << ")\n";
            return false;
        }

        double diff = std::fabs(tw_val - vm_res.value);
        if (diff > TOLERANCE)
        {
            std::cerr << "  WARNING: " << case_name
                      << " -- mismatch at t=" << sample_time
                      << " (public path=" << tw_val
                      << ", vm=" << vm_res.value
                      << ", diff=" << diff << ")\n";
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Printing helpers
// ---------------------------------------------------------------------------

static void print_header(double bpm, double ts_num, double ts_den)
{
    std::cout << "\n=== Bytecode VM Benchmark ===\n"
              << "BPM: " << bpm
              << ", Time Sig: " << static_cast<int>(ts_num)
              << "/" << static_cast<int>(ts_den) << "\n\n";

    std::cout << std::left << std::setw(30) << "Expression"
              << std::right << std::setw(16) << "Public path (us)"
              << std::setw(14) << "VM (us)"
              << std::setw(12) << "Speedup" << "\n";

    // separator
    for (int i = 0; i < 72; ++i)
        std::cout << '-';
    std::cout << "\n";
}

static void print_row(const char* name, double tw_us, double vm_us)
{
    double speedup = (vm_us > 0.0) ? tw_us / vm_us : 0.0;
    std::cout << std::left << std::setw(30) << name
              << std::right << std::fixed << std::setprecision(1)
              << std::setw(16) << tw_us
              << std::setw(14) << vm_us
              << std::setw(10) << speedup << "x\n";
}

static void print_skipped(const char* name)
{
    std::cout << std::left << std::setw(30) << name
              << std::right << std::setw(16) << "---"
              << std::setw(14) << "---"
              << std::setw(10) << "SKIP" << "\n";
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main()
{
    constexpr double BPM        = 120.0;
    constexpr double TS_NUM     = 4.0;
    constexpr double TS_DEN     = 4.0;
    constexpr double BEAT_DUR   = 60.0 / BPM;           // 0.5 s
    constexpr double BAR_DUR    = BEAT_DUR * TS_NUM;     // 2.0 s

    // --- Interpreter setup ---
    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();
    interp.set_bpm(BPM, 0.0);
    interp.set_time_sig(TS_NUM, TS_DEN);

    print_header(BPM, TS_NUM, TS_DEN);

    double log_sum     = 0.0;
    int    bench_count = 0;

    for (size_t ci = 0; ci < NUM_CASES; ++ci)
    {
        const BenchCase& bc = cases[ci];

        // 1. Eval setup code (binds expression to output a1)
        interp.clear_diagnostics();
        interp.eval(String(bc.setup_code));
        if (interp.has_diagnostics_error())
        {
            std::cerr << "  WARNING: " << bc.name
                      << " -- setup eval failed, skipping\n";
            print_skipped(bc.name);
            continue;
        }

        // 2. Compile to VM program
        Value expr = interp.get_parser()->parse(String(bc.expr));
        NumericVmCompileResult compiled =
            compile_numeric_program(expr, *interp.get_environment(), false);
        if (!compiled.ok)
        {
            std::cerr << "  WARNING: " << bc.name
                      << " -- VM compile failed ("
                      << compiled.error.c_str() << "), skipping\n";
            print_skipped(bc.name);
            continue;
        }

        // 3. Correctness check
        bool agree = verify_agreement(interp, compiled.program, "a1",
                                      BEAT_DUR, BAR_DUR, bc.name);
        if (!agree)
        {
            print_skipped(bc.name);
            continue;
        }

        // 4. Benchmark
        double tw_us = bench_public_output_path(interp, "a1", bc.iterations);
        double vm_us = bench_vm(compiled.program, bc.iterations,
                                BEAT_DUR, BAR_DUR);

        print_row(bc.name, tw_us, vm_us);

        if (vm_us > 0.0)
        {
            log_sum += std::log(tw_us / vm_us);
            ++bench_count;
        }
    }

    // Summary
    std::cout << "\n";
    if (bench_count > 0)
    {
        double geomean = std::exp(log_sum / bench_count);
        std::cout << "Geometric mean speedup: " << std::fixed
                  << std::setprecision(1) << geomean << "x"
                  << "  (" << bench_count << " cases)\n";
    }
    else
    {
        std::cout << "No benchmark cases ran successfully.\n";
    }

    return 0;
}
