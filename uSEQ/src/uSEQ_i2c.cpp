#include "uSEQ.h"
#include "hardware_includes.h"
#include "utils.h"

Value uSEQ::useq_i2c_send_to(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "send-to";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // if (!(args[0].is_string() || args[0].is_number()))
    // {
    //     report_error_wrong_specific_pred(
    //         user_facing_name, 0, "a string (module name) or a number (I2C index)",
    //         args[0].display());
    //     return Value::error();
    // }

    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    int i2c_idx = -1;

    // if (args[0].is_string())
    // {
    //     i2c_idx = i2c_idx_map[args[0].as_string()];
    // }
    // else
    // {
    i2c_idx = args[0].as_int();
    // }

    // the body is everything after the first arg
    // Value body = Value::vector({ args.data() + 1, args.size() - 1 });
    // NOTE: for now, the body is expected to be just one expr
    // if multiple expressions needed, use a do block
    Value body = args[1];

    String body_str = "@" + body.to_lisp_src();

    i2cWriteString(i2c_idx, body_str);

    println("String being sent to i2c: ");
    println(body_str);

    return result;
}

Value uSEQ::useq_i2c_host_start(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "i2c-host-start";

    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, -1);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    setup_i2cHOST();

    return result;
}