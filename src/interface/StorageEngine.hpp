#ifndef STORAGE_ENGINE_HPP
#define STORAGE_ENGINE_HPP

#include "common/Types.hpp"
#include "common/Error.hpp"
#include <expected>
#include <optional>

namespace zdb {

class StorageEngine {
public:
    virtual ~StorageEngine() = default;

    virtual std::expected<std::optional<Value>, Error> get(const Key& key) = 0;
    virtual std::expected<void, Error> set(const Key& key, const Value& value) = 0;
    virtual std::expected<std::optional<Value>, Error> erase(const Key& key) = 0;
};

} // namespace zdb

#endif // STORAGE_ENGINE_HPP
