#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <ostream>
#include <sstream>
#include <unordered_set>

namespace zdb {

enum class ErrorCode : char {
    OK,
    InvalidArg,
    ServiceTemporarilyUnavailable,
    AllServicesUnavailable,
    VersionMismatch,
    Maybe,
    KeyNotFound,
    TimeOut,
    Unknown
};

std::unordered_set<ErrorCode> retriableErrorCodes();
bool isRetriable(const ErrorCode& code);

std::ostream& operator<<(std::ostream& os, const ErrorCode& code);

std::string toString(const ErrorCode& code);

struct Error {
    ErrorCode code;
    std::string what;

    Error(const ErrorCode& c, std::string w);
    explicit Error(const ErrorCode& c);
};

} // namespace zdb

#endif // ERROR_H
