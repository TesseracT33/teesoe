#pragma once

#include <string>
#include <string_view>

class Status {
public:
    enum class Code {
        Ok,
        Failure,
        FileNotFound
    };

    constexpr Status(Code code) : code_(code){};
    constexpr Status(Code code, std::string_view message) : code_(code), message_(message){};

    constexpr Code code() const { return code_; }
    constexpr bool failure() const { return code_ == Code::Failure; }
    constexpr bool ok() const { return code_ == Code::Ok; }

private:
    Code code_;
    std::string message_;
};

constexpr Status status_ok(std::string_view message = {});
constexpr Status status_failure(std::string_view message);

constexpr std::string_view code_to_string(Status::Code code)
{
    switch (code) {
    case Status::Code::Ok: return "Ok";
    case Status::Code::Failure: return "Failure";
    case Status::Code::FileNotFound: return "FileNotFound";
    default: return "Unknown";
    }
}
