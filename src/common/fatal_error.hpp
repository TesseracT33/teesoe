#pragma once

#include <cstdlib>
#include <print>
#include <source_location>
#include <string>

template<typename... Args>
[[gnu::cold, noreturn]] void
  fatal_impl(std::source_location location, std::format_string<Args...> format, Args&&... args)
{
    std::string format_buffer;
    std::format_to(std::back_inserter(format_buffer), format, std::forward<Args>(args)...);
    std::println("[FATAL] At {}({}:{}) '{}': {}",
      location.file_name(),
      location.line(),
      location.column(),
      location.function_name(),
      format_buffer);
    std::exit(EXIT_FAILURE);
}

#define FATAL(...) fatal_impl(std::source_location::current(), __VA_ARGS__)
