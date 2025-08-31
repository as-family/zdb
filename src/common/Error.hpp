#ifndef SRC_COMMON_ERROR_HPP
#define SRC_COMMON_ERROR_HPP

#include <string>
#include <ostream>
#include <cstdint>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <type_traits>

namespace zdb {

enum class ErrorCode {
    OK,
    InvalidArg,
    ServiceTemporarilyUnavailable,
    AllServicesUnavailable,
    VersionMismatch,
    Maybe,
    KeyNotFound,
    TimeOut,
    NotLeader,
    Internal,
    Unknown
};

struct ErrorCodeHash {
    std::size_t operator()(const ErrorCode& code) const noexcept {
        return std::hash<std::underlying_type_t<ErrorCode>>{}(static_cast<std::underlying_type_t<ErrorCode>>(code));
    }
};


extern const std::unordered_map<std::string, std::unordered_set<ErrorCode, ErrorCodeHash>> retriableErrorCodes;
bool isRetriable(const std::string& op, const ErrorCode& code);

std::ostream& operator<<(std::ostream& os, const ErrorCode& code);

std::string toString(const ErrorCode& code);

struct Error {
    ErrorCode code;
    std::string what;
    std::string key;
    std::string value;
    uint64_t version;

    Error(const ErrorCode& c, std::string w);
    Error(const ErrorCode& c, std::string w, std::string k, std::string v, uint64_t ver);
    explicit Error(const ErrorCode& c);
};

} // namespace zdb

#endif // SRC_COMMON_ERROR_HPP
