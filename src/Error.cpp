#include "Error.hpp"

namespace zdb {

std::ostream& operator<<(std::ostream& os, ErrorCode code) {
    os << toString(code);
    return os;
}

std::string toString(ErrorCode code) {
    switch (code)
    {
        case ErrorCode::NotFound: return "Not Found";
        case ErrorCode::InvalidArg: return "Invalid Argument";
    }
}


Error::Error(ErrorCode c, std::string w) : code {c}, what {w} {}
Error::Error(ErrorCode c) : code {c}, what {toString(c)} {}

} // namespace zdb
