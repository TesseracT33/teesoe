#pragma once

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

enum class LogLevel {
    Fatal,
    Error,
    Warn,
    Info,
    Debug,
};

void DoLog(std::string_view output, LogLevel level);
void SetLogModeConsole();
void SetLogModeFile(std::filesystem::path const& path);
void TearDownLog();

template<typename... Args> void LogDebug(std::format_string<Args...> format, Args&&... args)
{
    std::string format_buffer;
    std::format_to(std::back_inserter(format_buffer), format, std::forward<Args>(args)...);
    DoLog(format_buffer, LogLevel::Debug);
}

template<typename... Args> void LogError(std::format_string<Args...> format, Args&&... args)
{
    std::string format_buffer;
    std::format_to(std::back_inserter(format_buffer), format, std::forward<Args>(args)...);
    DoLog(format_buffer, LogLevel::Error);
}

template<typename... Args> void LogFatal(std::format_string<Args...> format, Args&&... args)
{
    std::string format_buffer;
    std::format_to(std::back_inserter(format_buffer), format, std::forward<Args>(args)...);
    DoLog(format_buffer, LogLevel::Fatal);
}

template<typename... Args> void LogInfo(std::format_string<Args...> format, Args&&... args)
{
    std::string format_buffer;
    std::format_to(std::back_inserter(format_buffer), format, std::forward<Args>(args)...);
    DoLog(format_buffer, LogLevel::Info);
}

template<typename... Args> void LogWarn(std::format_string<Args...> format, Args&&... args)
{
    std::string format_buffer;
    std::format_to(std::back_inserter(format_buffer), format, std::forward<Args>(args)...);
    DoLog(format_buffer, LogLevel::Warn);
}

template<typename Str>
void LogDebug(Str str)
    requires std::convertible_to<Str, std::string_view>
{
    DoLog(str, LogLevel::Debug);
}

template<typename Str>
void LogError(Str str)
    requires std::convertible_to<Str, std::string_view>
{
    DoLog(str, LogLevel::Error);
}

template<typename Str>
void LogFatal(Str str)
    requires std::convertible_to<Str, std::string_view>
{
    DoLog(str, LogLevel::Fatal);
}

template<typename Str>
void LogInfo(Str str)
    requires std::convertible_to<Str, std::string_view>
{
    DoLog(str, LogLevel::Info);
}

template<typename Str>
void LogWarn(Str str)
    requires std::convertible_to<Str, std::string_view>
{
    DoLog(str, LogLevel::Warn);
}
