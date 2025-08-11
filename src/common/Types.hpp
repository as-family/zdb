#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include <cstdint>

namespace zdb {

struct Key {
    std::string data;
};

struct Value {
    std::string data;
    uint64_t version;
};


} // namespace zdb

#endif // TYPES_HPP
