#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <ostream>
#include <sstream>
#include <unordered_set>

namespace zdb {

enum class ErrorCode {
    NotFound,
    InvalidArg,
    ServiceTemporarilyUnavailable,
    AllServicesUnavailable,
    Unknown
};

std::unordered_set<ErrorCode> retriableErrorCodes();
bool isRetriable(ErrorCode code);

std::ostream& operator<<(std::ostream& os, ErrorCode code);

std::string toString(ErrorCode code);

struct Error {
    ErrorCode code;
    std::string what;

    Error(ErrorCode c, std::string w);
    explicit Error(ErrorCode c);
};

} // namespace zdb

#endif // ERROR_H
