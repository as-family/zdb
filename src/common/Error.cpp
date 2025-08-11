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
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::NotFound: return "Not Found";
        case ErrorCode::InvalidArg: return "Invalid Argument";
        case ErrorCode::ServiceTemporarilyUnavailable: return "Service Temporarily Unavailable";
        case ErrorCode::AllServicesUnavailable: return "All Services Unavailable";
        case ErrorCode::VersionMismatch: return "Version Mismatch";
    }
    std::unreachable();
}

std::unordered_set<ErrorCode> retriableErrorCodes() {
    return {ErrorCode::Unknown, ErrorCode::ServiceTemporarilyUnavailable, ErrorCode::AllServicesUnavailable};
}

bool isRetriable(const ErrorCode& code) {
    return retriableErrorCodes().contains(code);
}


Error::Error(const ErrorCode& c, std::string w) : code {c}, what {std::move(w)} {}
Error::Error(const ErrorCode& c) : code {c}, what {toString(c)} {}

} // namespace zdb
