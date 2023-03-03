#pragma once

#include <source_location>
#include <string_view>

class Status;

Status init_file_log();
void log(std::string_view out);
void log_error(std::string_view out);
void log_fatal(std::string_view out /*, std::source_location loc = std::source_location::current()*/);
void log_info(std::string_view out);
void log_warn(std::string_view out);
