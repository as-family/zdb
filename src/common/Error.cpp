#include "Error.hpp"
#include <utility>

namespace zdb {

std::ostream& operator<<(std::ostream& os, ErrorCode code) {
    os << toString(code);
    return os;
}

std::string toString(ErrorCode code) {
    switch (code)
    {
        case ErrorCode::Unknown: return "Unknown";
        case ErrorCode::NotFound: return "Not Found";
        case ErrorCode::InvalidArg: return "Invalid Argument";
        case ErrorCode::ServiceTemporarilyUnavailable: return "Service Temporarily Unavailable";
    }
    std::unreachable();
}

std::unordered_set<ErrorCode> retriableErrorCodes() {
    return {ErrorCode::Unknown, ErrorCode::ServiceTemporarilyUnavailable};
}

bool isRetriable(ErrorCode code) {
    return retriableErrorCodes().contains(code);
}


Error::Error(ErrorCode c, std::string w) : code {c}, what {w} {}
Error::Error(ErrorCode c) : code {c}, what {toString(c)} {}

} // namespace zdb
