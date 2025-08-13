#include "Error.hpp"
#include <utility>
#include <string>
#include <ostream>
#include <unordered_set>

namespace zdb {

std::ostream& operator<<(std::ostream& os, const ErrorCode& code) {
    os << toString(code);
    return os;
}

std::string toString(const ErrorCode& code) {
    switch (code)
    {
        case ErrorCode::OK: return "OK";
        case ErrorCode::TimeOut: return "Time Out";
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::InvalidArg: return "Invalid Argument";
        case ErrorCode::ServiceTemporarilyUnavailable: return "Service Temporarily Unavailable";
        case ErrorCode::AllServicesUnavailable: return "All Services Unavailable";
        case ErrorCode::VersionMismatch: return "Version Mismatch";
        case ErrorCode::Maybe: return "Maybe";
        case ErrorCode::KeyNotFound: return "Key Not Found";
    }
    std::unreachable();
}

std::unordered_set<ErrorCode> retriableErrorCodes() {
    return {
        ErrorCode::Unknown,
        ErrorCode::ServiceTemporarilyUnavailable,
        ErrorCode::AllServicesUnavailable,
        ErrorCode::TimeOut
    };
}

bool isRetriable(const ErrorCode& code) {
    return retriableErrorCodes().contains(code);
}


Error::Error(const ErrorCode& c, std::string w) : code {c}, what {std::move(w)} {}
Error::Error(const ErrorCode& c) : code {c}, what {toString(c)} {}

} // namespace zdb
