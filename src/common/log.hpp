#pragma once

#include <source_location>
#include <string_view>

class Status;

Status InitFileLog();
void Log(std::string_view out);
void LogError(std::string_view out);
void LogFatal(std::string_view out /*, std::source_location loc = std::source_location::current()*/);
void LogInfo(std::string_view out);
void LogWarn(std::string_view out);
void TearDownLog();
