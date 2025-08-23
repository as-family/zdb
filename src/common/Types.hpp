#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include <cstdint>
#include "proto/types.pb.h"
#include "raft/Command.hpp"

namespace zdb {

struct Key {
    std::string data;
    
    Key(const std::string& d) : data(d) {}
    
    Key(const proto::Key& protoKey);

    bool operator==(const Key& other) const {
        return data == other.data;
    }
};

struct KeyHash {
    std::size_t operator()(const Key& key) const {
        return std::hash<std::string>()(key.data);
    }
};

struct Value {
    std::string data;
    uint64_t version = 0;
    
    Value(const std::string& d, uint64_t v = 0) : data(d), version(v) {}
    
    Value(const proto::Value& protoValue);

    bool operator==(const Value& other) const {
        return data == other.data && version == other.version;
    }
};

struct Command : public raft::Command {
    std::string name;
    std::vector<std::string> args;

    Command(const std::string& n, const std::vector<std::string>& a)
        : name(n), args(a) {}
    Command(const std::string& n)
        : name(n), args {} {}

    void apply() override {
        // Implement the command application logic here
    }
    std::string serialize() const override {
        return name;
    }
};

} // namespace zdb

#endif // TYPES_HPP
