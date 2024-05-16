#pragma once

#include <format>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

class Status {
public:
    enum class Code {
        Ok,
        Failure,
        NotFound,
        Unimplemented,
    };

    Status(Code code) : code_(code) {}
    Status(Code code, std::string message) : code_(code), message_(std::move(message)) {}

    std::string_view Message() const { return message_; }
    bool Ok() const { return code_ == Code::Ok; }

private:
    Code code_;
    std::string message_;
};

inline Status OkStatus()
{
    return Status(Status::Code::Ok);
}

inline Status FailureStatus(std::string message = {})
{
    return Status(Status::Code::Failure, message);
}

inline Status NotFoundStatus(std::string message = {})
{
    return Status(Status::Code::NotFound, message);
}

inline Status UnimplementedStatus(std::string message = {})
{
    return Status(Status::Code::Unimplemented, message);
}

template<typename... Args> Status FailureStatus(std::format_string<Args...> fmt, Args&&... args)
{
    std::string message;
    (void)std::format_to(std::back_inserter(message), std::move(fmt), std::forward<Args>(args)...);
    return Status(Status::Code::Failure, std::move(message));
}

template<typename... Args> Status NotFoundStatus(std::format_string<Args...> fmt, Args&&... args)
{
    std::string message;
    (void)std::format_to(std::back_inserter(message), std::move(fmt), std::forward<Args>(args)...);
    return Status(Status::Code::NotFound, std::move(message));
}

template<typename... Args> Status UnimplementedStatus(std::format_string<Args...> fmt, Args&&... args)
{
    std::string message;
    (void)std::format_to(std::back_inserter(message), std::move(fmt), std::forward<Args>(args)...);
    return Status(Status::Code::Unimplemented, std::move(message));
}

constexpr std::string_view CodeToStr(Status::Code code)
{
    switch (code) {
    case Status::Code::Ok: return "Ok";
    case Status::Code::Failure: return "Failure";
    case Status::Code::NotFound: return "NotFound";
    case Status::Code::Unimplemented: return "Unimplemented";
    default: throw std::invalid_argument("Unknown status code given to CodeToStr");
    }
}
