#include "common/Types.hpp"
#include "proto/types.pb.h"

namespace zdb {

Key::Key(const proto::Key& protoKey) 
    : data(protoKey.data()) {
}

Value::Value(const proto::Value& protoValue) 
    : data(protoValue.data()), version(protoValue.version()) {
}

} // namespace zdb
