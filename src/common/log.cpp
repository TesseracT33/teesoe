#include "log.hpp"
#include "build_options.hpp"
#include "numtypes.hpp"
#include "status.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

static void FileOut(std::string_view output);
static void StdOut(std::string_view output);

static std::ofstream file_log;
[[maybe_unused]] static u64 file_output_repeat_counter;
[[maybe_unused]] static u32 loop_index;
static std::string prev_file_output;
static std::vector<std::string> file_output_loop;

void FileOut(std::string_view output)
{
    if constexpr (enable_file_logging) {
        if (!file_log.is_open()) {
            return;
        }
        /*auto NewLoop = [=] {
            file_output_loop.clear();
            file_output_loop.emplace_back(output);
            loop_index = 0;
            file_log << output << '\n';
        };
        auto OnLoopClosed = [=] {
            if (file_output_repeat_counter > 0) {
                file_log << std::format("<<< {} line(s) repeated {} time(s) >>>\n",
                  file_output_loop.size(),
                  file_output_repeat_counter);
                file_output_repeat_counter = 0;
            }
            NewLoop();
        };
        if (file_output_loop.empty()) {
            NewLoop();
        } else {
            auto it = std::ranges::find(file_output_loop, output);
            if (it == file_output_loop.end()) {
                if (file_output_loop.size() < 8) {
                    file_output_loop.emplace_back(output);
                    file_log << output << '\n';
                } else {
                    OnLoopClosed();
                }
            } else if (std::distance(file_output_loop.begin(), it) == loop_index) {
                loop_index = (loop_index + 1) % file_output_loop.size();
                if (loop_index == 0) {
                    file_output_repeat_counter++;
                }
            } else {
                if (loop_index == 0) {
                    OnLoopClosed();
                } else {

                }

            }
        }*/
        if (output == prev_file_output) {
            ++file_output_repeat_counter;
        } else {
            if (file_output_repeat_counter > 0) {
                file_log << "<<< Repeated " << file_output_repeat_counter << " time(s). >>>\n";
                file_output_repeat_counter = 0;
            }
            prev_file_output = output;
            file_log << output << std::endl;
        }
    }
}

Status InitFileLog()
{
    if constexpr (enable_file_logging) {
        file_log.open(log_path.data());
        return file_log.is_open() ? OkStatus() : FailureStatus("Failed to open log file");
    } else {
        return OkStatus();
    }
}

std::ofstream const& GetFileLogHandle()
{
    return file_log;
}

void Log(std::string_view output)
{
    StdOut(output);
    FileOut(output);
}

void LogError(std::string_view output)
{
    std::string shown_output = std::format("[ERROR] {}", output);
    StdOut(shown_output);
    FileOut(shown_output);
}

// TODO: std::source_location not working with clang-cl for some obscure reason
void LogFatal(std::string_view output /*, std::source_location loc*/)
{
    // std::string shown_output = std::format("[FATAL] {}({}:{}), function {}: {}",
    //   loc.file_name(),
    //   loc.line(),
    //   loc.column(),
    //   loc.function_name(),
    //   output);
    std::string shown_output = std::format("[FATAL] {}", output);
    StdOut(shown_output);
    FileOut(shown_output);
}

void LogInfo(std::string_view output)
{
    std::string shown_output = std::format("[INFO] {}", output);
    StdOut(shown_output);
    FileOut(shown_output);
}

void LogWarn(std::string_view output)
{
    std::string shown_output = std::format("[WARN] {}", output);
    StdOut(shown_output);
    FileOut(shown_output);
}

void StdOut(std::string_view output)
{
    if constexpr (enable_console_logging) {
        std::cout << output << '\n';
    }
}

void TearDownLog()
{
    if (file_log) {
        std::flush(file_log);
    }
}
