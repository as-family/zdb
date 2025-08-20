#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>

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
    Unknown
};

extern std::unordered_map<std::string, std::unordered_set<ErrorCode>> retriableErrorCodes;
bool isRetriable(std::string op, const ErrorCode& code);

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

#endif // ERROR_H
