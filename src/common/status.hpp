#pragma once

#include <string>
#include <string_view>

class Status {
public:
    enum class Code {
        Ok,
        Failure,
        FileNotFound,
        Unimplemented
    };

    constexpr Status(Code code) : code_(code) {}
    constexpr Status(Code code, std::string_view message) : code_(code), message_(message) {}

    constexpr std::string_view Message() const { return message_; }
    constexpr bool Ok() const { return code_ == Code::Ok; }

private:
    Code code_;
    std::string message_;
};

constexpr Status OkStatus(std::string_view message = {})
{
    return Status(Status::Code::Ok, message);
}

constexpr Status FailureStatus(std::string_view message)
{
    return Status(Status::Code::Failure, message);
}

constexpr Status UnimplementedStatus(std::string_view message = {})
{
    return Status(Status::Code::Unimplemented, message);
}

constexpr std::string_view CodeToString(Status::Code code)
{
    switch (code) {
    case Status::Code::Ok: return "Ok";
    case Status::Code::Failure: return "Failure";
    case Status::Code::FileNotFound: return "FileNotFound";
    default: return "Unknown";
    }
}
