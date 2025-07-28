#ifndef ERROR_H
#define ERROR_H

#include <string>
#include <iostream>
#include <sstream>

namespace zdb {

enum class ErrorCode {
    Unknown,
    NotFound,
    InvalidArg
};

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
