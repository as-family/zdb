#ifndef TYPES_HPP
#define TYPES_HPP

#include <string>
#include <cstdint>
#include "proto/types.pb.h"
#include "raft/StateMachine.hpp"
#include <expected>
#include <optional>
#include "common/Error.hpp"
#include <variant>

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

struct State : public raft::State {
    Key key;
    std::variant<
        std::expected<std::optional<Value>, Error>,
        std::expected<std::monostate, Error>
    > u;
    State(const Key& k, const std::expected<std::optional<Value>, Error>& v)
        : key(k), u{v} {}
    State(const Key& k, const std::expected<std::monostate, Error>& v)
        : key(k), u{v} {}
};

} // namespace zdb

#endif // TYPES_HPP
