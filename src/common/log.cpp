#include "log.hpp"
#include "build_options.hpp"
#include "status.hpp"
#include "types.hpp"
#include "util.hpp"

#include <format>
#include <fstream>
#include <iostream>
#include <string>

static void file_out(std::string_view output);
static void std_out(std::string_view output);

static std::ofstream file_log;
static std::string prev_file_output;
static u64 file_output_repeat_counter;

void log(std::string_view output)
{
    std_out(output);
    file_out(output);
}

void log_error(std::string_view output)
{
    std::string shown_output = std::format("[ERROR] {}", output);
    std_out(shown_output);
    file_out(shown_output);
}

// TODO: std::source_location not working with clang-cl for some obscure reason
void log_fatal(std::string_view output /*, std::source_location loc*/)
{
    // std::string shown_output = std::format("[FATAL] {}({}:{}), function {}: {}",
    //   loc.file_name(),
    //   loc.line(),
    //   loc.column(),
    //   loc.function_name(),
    //   output);
    std::string shown_output = std::format("[FATAL] {}", output);
    std_out(shown_output);
    file_out(shown_output);
}

void file_out(std::string_view output)
{
    if constexpr (enable_file_logging) {
        if (!file_log.is_open()) {
            return;
        }
        if (output == prev_file_output) {
            ++file_output_repeat_counter;
        } else {
            if (file_output_repeat_counter > 0) {
                file_log << "<<< Repeated " << file_output_repeat_counter << " time(s). >>>\n";
                file_output_repeat_counter = 0;
            }
            prev_file_output = output;
            file_log << output << '\n';
        }
    }
}

void log_info(std::string_view output)
{
    std::string shown_output = std::format("[INFO] {}", output);
    std_out(shown_output);
    file_out(shown_output);
}

Status init_file_log()
{
    if constexpr (enable_file_logging) {
        file_log.open(log_path.data());
        return file_log.is_open() ? status_ok() : status_failure("Failed to open log file");
    } else {
        return status_ok();
    }
}

void std_out(std::string_view output)
{
    std::cout << output << '\n';
}

void log_warn(std::string_view output)
{
    std::string shown_output = std::format("[WARN] {}", output);
    std_out(shown_output);
    file_out(shown_output);
}
