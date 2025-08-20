#include "Error.hpp"
#include <utility>
#include <string>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

namespace zdb {

std::ostream& operator<<(std::ostream& os, const ErrorCode& code) {
    os << toString(code);
    return os;
}

std::string toString(const ErrorCode& code) {
    switch (code)
    {
        case ErrorCode::OK: return "OK";
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

std::unordered_map<std::string, std::unordered_set<ErrorCode>> retriableErrorCodes = {
    {"erase", {
        ErrorCode::ServiceTemporarilyUnavailable,
        ErrorCode::AllServicesUnavailable,
    }},
    {"default", {
        ErrorCode::Unknown,
        ErrorCode::ServiceTemporarilyUnavailable,
        ErrorCode::AllServicesUnavailable,
        ErrorCode::TimeOut,
    }}
};

bool isRetriable(std::string op, const ErrorCode& code) {
    auto it = retriableErrorCodes.find(op);
    if (it != retriableErrorCodes.end()) {
        return it->second.contains(code);
    } else {
        auto d = retriableErrorCodes.find("default");
        return d->second.contains(code);
    }
}

Error::Error(const ErrorCode& c, std::string w, std::string k, std::string v, uint64_t ver) : code {c}, what {w}, key {k}, value {v}, version {ver} {}
Error::Error(const ErrorCode& c, std::string w) : code {c}, what {w}, key{}, value{}, version{} {}
Error::Error(const ErrorCode& c) : code {c}, what {toString(c)}, key{}, value{}, version{} {}

} // namespace zdb
