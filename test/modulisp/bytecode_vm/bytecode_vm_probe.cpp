#include "../../../uSEQ/src/modulisp/diagnostic.h"
#include "../../../uSEQ/src/modulisp/modulisp_interpreter.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace
{
struct Options
{
    std::string code;
    std::string output = "a1";
    double time_seconds = 0.0;
    double bpm = 120.0;
    double time_sig_num = 4.0;
    double time_sig_den = 4.0;
    bool smoke = false;
};

std::string json_escape(const std::string& value)
{
    std::ostringstream out;
    for (char c : value)
    {
        switch (c)
        {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << c;
            break;
        }
    }
    return out.str();
}

void print_json(bool ok, const std::string& mode, const std::string& output,
                double time_seconds, double value,
                const std::string& error = std::string())
{
    std::cout << std::setprecision(15) << "{"
              << "\"ok\":" << (ok ? "true" : "false") << ","
              << "\"mode\":\"" << json_escape(mode) << "\","
              << "\"output\":\"" << json_escape(output) << "\","
              << "\"time_seconds\":" << time_seconds << ",";

    if (ok && std::isfinite(value))
    {
        std::cout << "\"value\":" << value;
    }
    else
    {
        std::cout << "\"value\":null";
    }

    if (!error.empty())
    {
        std::cout << ",\"error\":\"" << json_escape(error) << "\"";
    }

    std::cout << "}" << std::endl;
}

std::string error_message(const ModuLispInterpreter& interp)
{
    const auto& diags = interp.get_diagnostics();
    if (diags.empty())
    {
        return std::string();
    }

    // Return the last error-severity diagnostic message
    for (auto it = diags.rbegin(); it != diags.rend(); ++it)
    {
        if (it->severity == DiagnosticSeverity::Error)
        {
            return it->message.c_str();
        }
    }

    return std::string();
}

Options parse_args(int argc, char* argv[])
{
    Options opts;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h")
        {
            std::cout << "Usage: bytecode_vm_probe [--code EXPR] [--output NAME] "
                         "[--time SECONDS] [--bpm BPM] [--time-sig NUM DEN]\n";
            std::exit(0);
        }
        if (arg == "--smoke")
        {
            opts.smoke = true;
            continue;
        }
        if (arg == "--code" && i + 1 < argc)
        {
            opts.code = argv[++i];
            continue;
        }
        if (arg == "--output" && i + 1 < argc)
        {
            opts.output = argv[++i];
            continue;
        }
        if (arg == "--time" && i + 1 < argc)
        {
            opts.time_seconds = std::atof(argv[++i]);
            continue;
        }
        if (arg == "--bpm" && i + 1 < argc)
        {
            opts.bpm = std::atof(argv[++i]);
            continue;
        }
        if (arg == "--time-sig" && i + 2 < argc)
        {
            opts.time_sig_num = std::atof(argv[++i]);
            opts.time_sig_den = std::atof(argv[++i]);
            continue;
        }

        std::cerr << "Unknown argument: " << arg << std::endl;
        std::exit(2);
    }
    if (opts.code.empty())
    {
        opts.smoke = true;
    }
    return opts;
}
} // namespace

int main(int argc, char* argv[])
{
    const Options opts = parse_args(argc, argv);
    const std::string smoke_code = "(a1 (+ 1 2))";

    ModuLispInterpreter interp(nullptr, nullptr, nullptr, 8, 8, 8);
    interp.init();
    interp.set_bpm(opts.bpm, 0.0);
    interp.set_time_sig(opts.time_sig_num, opts.time_sig_den);

    const std::string& code = opts.smoke ? smoke_code : opts.code;
    if (code.empty())
    {
        std::cerr << "Missing --code expression" << std::endl;
        print_json(false, "error", opts.output, opts.time_seconds, std::numeric_limits<double>::quiet_NaN(),
                   "missing code");
        return 2;
    }

    interp.clear_diagnostics();
    String result = interp.eval(String(code.c_str()));
    if (interp.has_diagnostics_error())
    {
        const std::string message = error_message(interp);
        print_json(false, opts.smoke ? "smoke" : "probe", opts.output,
                   opts.time_seconds, std::numeric_limits<double>::quiet_NaN(),
                   message);
        return 1;
    }

    (void)result;

    bool ok = false;
    const double value =
        interp.eval_output_at_time(opts.output.c_str(), opts.time_seconds, &ok);
    if (!ok || !std::isfinite(value))
    {
        print_json(false, opts.smoke ? "smoke" : "probe", opts.output,
                   opts.time_seconds, value, "output evaluation failed");
        return 1;
    }

    if (opts.smoke)
    {
        const bool smoke_ok = std::fabs(value - 3.0) < 1e-9;
        print_json(smoke_ok, "smoke", opts.output, opts.time_seconds, value,
                   smoke_ok ? std::string() : "smoke assertion failed");
        return smoke_ok ? 0 : 1;
    }

    print_json(true, "probe", opts.output, opts.time_seconds, value);
    return 0;
}
