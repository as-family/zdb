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
        case ErrorCode::TimeOut: return "TimeOut";
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::InvalidArg: return "InvalidArgument";
        case ErrorCode::ServiceTemporarilyUnavailable: return "ServiceTemporarilyUnavailable";
        case ErrorCode::AllServicesUnavailable: return "AllServicesUnavailable";
        case ErrorCode::VersionMismatch: return "VersionMismatch";
        case ErrorCode::Maybe: return "Maybe";
        case ErrorCode::KeyNotFound: return "KeyNotFound";
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
