#include "log.hpp"
#include "numtypes.hpp"

#include <array>
#include <cstdio>
#include <print>

enum class LogMode {
    Console,
    File,
};

static FILE* stream = stdout;
static LogMode log_mode = LogMode::Console;

void DoLog(std::string_view output, LogLevel level)
{
    static u64 output_repeat_count;
    static std::string prev_output;

    // todo: also handle small loops
    if (output == prev_output) {
        output_repeat_count++;
    } else {
        if (output_repeat_count > 0) {
            std::println(stream, "<<< Repeated {} time(s) >>>", output_repeat_count);
            output_repeat_count = 0;
        }
        prev_output = output;
        static constexpr std::array log_level_str = { "[FATAL]", "[ERROR]", "[WARN]", "[INFO]", "[DEBUG]" };
        std::println(stream, "{} {}", log_level_str[std::to_underlying(level)], output);
    }
}

void SetLogModeConsole()
{
    if (std::exchange(log_mode, LogMode::Console) == LogMode::File) {
        fclose(stream);
    }
    stream = stdout;
}

void SetLogModeFile(std::filesystem::path const& path)
{
    if (std::exchange(log_mode, LogMode::File) == LogMode::File) {
        fclose(stream);
    }
    // TODO: Use fstream instead, and fstream::native_handle () to get the FILE* handle
    char const* cstr = path.string().c_str();
    stream = fopen(cstr, "w");
    if (stream) {
        log_mode = LogMode::File;
    } else {
        log_mode = LogMode::Console;
        stream = stdout;
        LogError("Failed to open log file at {}", cstr);
    }
}

void TearDownLog()
{
    if (log_mode == LogMode::File) {
        fclose(stream);
    }
}
